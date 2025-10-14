#include <ctype.h>
#include <dirent.h>

#include "panel-gaki.h"
#include "gaki.h"

void panel_gaki_select_up(Panel_Gaki *st, size_t n) {
    if(!st->panel_file) return;
    if(!st->panel_file->select) {
        st->panel_file->select = file_infos_length(st->panel_file->file_infos) - 1;
        st->panel_file->offset = st->panel_file->select + 1 > st->layout.rc_files.dim.y ? st->panel_file->select + 1 - st->layout.rc_files.dim.y : 0;
    } else {
        --st->panel_file->select;
        if(st->panel_file->offset) {
            --st->panel_file->offset;
        }
    }
}

void panel_gaki_select_down(Panel_Gaki *st, size_t n) {
    if(!st->panel_file) return;
    ++st->panel_file->select;
    if(st->panel_file->select >= file_infos_length(st->panel_file->file_infos)) {
        st->panel_file->select = 0;
        st->panel_file->offset = 0;
    }
    if(st->panel_file->select >= st->panel_file->offset + st->layout.rc_files.dim.y) {
        ++st->panel_file->offset;
    }
}

void panel_gaki_select_at(Panel_Gaki *st, size_t n) {
}

void panel_gaki_layout_from_rules(Panel_Gaki_Layout *layout, Panel_Gaki_Config *config) {

    layout->rc_files = config->rc;
    layout->rc_split = config->rc;
    layout->rc_preview = config->rc;
    layout->rc_pwd = config->rc;

    /* make space for bar */
    layout->rc_pwd.dim.y = 1;

    layout->rc_files.anc.y += 1;
    layout->rc_split.anc.y += 1;
    layout->rc_preview.anc.y += 1;

    layout->rc_files.dim.y -= 1;
    layout->rc_split.dim.y -= 1;
    layout->rc_preview.dim.y -= 1;

    /* files on left half */
    layout->rc_files.dim.x /= 2;

    /* split middle, 1 wide */
    layout->rc_split.anc.x = layout->rc_files.anc.x + layout->rc_files.dim.x;
    layout->rc_split.dim.x = 1;

    /* preview right half */
    layout->rc_preview.anc.x = layout->rc_split.anc.x + 1;
    layout->rc_preview.dim.x = config->rc.dim.x - layout->rc_files.dim.x - 1;
}

int panel_file_read_sync(So path, File_Infos *file_infos) {
    DIR *d;
    struct dirent *dir;
    char *cdirname = so_dup(path);
    char *cfilename = 0;
    d = opendir(cdirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            File_Info info = {0};
            info.filename = so_clone(so_l(dir->d_name));
            so_path_join(&info.path, path, info.filename);
            cfilename = so_dup(info.path);
            stat(cfilename, &info.stats);
            file_infos_push_back(file_infos, &info);
        }
        closedir(d);
    }
    free(cdirname);
    free(cfilename);
    file_infos_sort(file_infos);
    return(0);
}


typedef struct Task_File_Info_Load_File {
    File_Info *current;
    Gaki *gaki;
} Task_File_Info_Load_File;

void *task_file_info_load_file(Pw *pw, bool *quit, void *void_task) {
    Task_File_Info_Load_File *task = void_task;

    So content = SO;
    bool printable = true;

    so_file_read(task->current->path, &content);
    for(size_t i = 0; i < so_len(content); ++i) {
        unsigned char c = so_at(content, i);
        if(!(c >= ' ' || isspace(c))) {
            printable = false;
            break;
        }
    }
    if(!printable) {
        so_free(&content);
    }

    pthread_mutex_lock(&task->gaki->panel_gaki.rwlock);
    task->current->content = content;
    task->current->printable = printable;
    pthread_mutex_unlock(&task->gaki->panel_gaki.rwlock);

    pthread_mutex_lock(&task->gaki->sync_main.mtx);
    ++task->gaki->sync_main.render_do;
    pthread_cond_signal(&task->gaki->sync_main.cond);
    pthread_mutex_unlock(&task->gaki->sync_main.mtx);

    free(task);
    return 0;
}

typedef struct Task_Panel_Gaki_Read_Dir {
    //Panel_Gaki *current;
    File_Infos *infos;
    So path;
    Gaki *gaki;
    bool *printable;
} Task_Panel_Gaki_Read_Dir;

void *task_panel_gaki_read_dir(Pw *pw, bool *quit, void *void_task) {
    Task_Panel_Gaki_Read_Dir *task = void_task;

    File_Infos file_infos = {0};
    panel_file_read_sync(task->path, &file_infos);

    pthread_mutex_lock(&task->gaki->panel_gaki.rwlock);
    *task->infos = file_infos;
    if(task->printable) *task->printable = true;
    pthread_mutex_unlock(&task->gaki->panel_gaki.rwlock);

    pthread_mutex_lock(&task->gaki->sync_main.mtx);
    ++task->gaki->sync_main.update_do;
    pthread_cond_signal(&task->gaki->sync_main.cond);
    pthread_mutex_unlock(&task->gaki->sync_main.mtx);

    free(task);
    return 0;
}


typedef struct Task_File_Info_Read_Dir {
    File_Info *current;
    Gaki *gaki;
} Task_File_Info_Read_Dir;

void *task_panel_file_read_dir(Pw *pw, bool *quit, void *void_task) {
    Task_File_Info_Read_Dir *task = void_task;

    Panel_File panel = {0};
    panel_file_read_sync(task->current->path, &panel.file_infos);

    pthread_mutex_lock(&task->gaki->panel_gaki.rwlock);
    *task->current->panel_file = panel;
    task->current->printable = true;
    pthread_mutex_unlock(&task->gaki->panel_gaki.rwlock);

    pthread_mutex_lock(&task->gaki->sync_main.mtx);
    ++task->gaki->sync_main.update_do;
    pthread_cond_signal(&task->gaki->sync_main.cond);
    pthread_mutex_unlock(&task->gaki->sync_main.mtx);

    free(task);
    return 0;
}


void panel_gaki_update(Panel_Gaki *st, Action *ac) {

    pthread_mutex_lock(&st->rwlock);

    panel_gaki_layout_from_rules(&st->layout, &st->config);

    if(ac->select_left) {
        So left = so_ensure_dir(so_rsplit_ch(st->pwd, PLATFORM_CH_SUBDIR, 0));
        if(left.len) {
            st->pwd = left;
            st->panel_file = 0;
        } else {
            so_copy(&st->pwd, so("/"));
            st->panel_file = 0;
        }
    }

    if(ac->select_right && st->panel_file) {
        if(st->panel_file->select < file_infos_length(st->panel_file->file_infos)) {
            File_Info *sel = file_infos_get_at(&st->panel_file->file_infos, st->panel_file->select);
            if(S_ISDIR(sel->stats.st_mode)) {
                so_path_join(&st->pwd, st->pwd, sel->filename);
                st->panel_file = 0;

            } else if(S_ISREG(sel->stats.st_mode)) {
                So ed = SO;
                char *ced = 0;
                so_env_get(&ed, so("EDITOR"));
                if(so_len(ed)) {
#if 0
                    so_fmt(&ed, " '%.*s'", SO_F(sel->path));
                    ced = so_dup(ed);
                    tui_exit();
                    system(ced);
                    tui_enter();
#endif
                } else {
                    printff("\rNO EDITOR FOUND");
                    exit(1);
                }
                so_free(&ed);
                free(ced);
            }
        }
    }

    if(!so_len(st->pwd)) {
        so_env_get(&st->pwd, so("PWD"));
    }

    if(!st->panel_file) {
        t_panel_file_ensure_exist(&st->t_file_infos, &st->panel_file, st->pwd);

        if(!st->panel_file->read) {
            st->panel_file->read = true;

            Task_Panel_Gaki_Read_Dir *task;
            NEW(Task_Panel_Gaki_Read_Dir, task);
            task->gaki = st->gaki;
            task->path = st->pwd;
            task->infos = &st->panel_file->file_infos;
            pw_queue(&st->gaki->pw_task, task_panel_gaki_read_dir, task);
        }
    }

    if(ac->select_up) {
        panel_gaki_select_up(st, ac->select_up);
    }

    if(ac->select_down) {
        panel_gaki_select_down(st, ac->select_down);
    }

    if(st->panel_file && st->panel_file->select < file_infos_length(st->panel_file->file_infos)) {
        File_Info *current = file_infos_get_at(&st->panel_file->file_infos, st->panel_file->select);
        if(!current->have_read) {
            current->have_read = true;
            if(S_ISREG(current->stats.st_mode)) {
                Task_File_Info_Load_File *task;
                NEW(Task_File_Info_Load_File, task);
                task->current = current;
                task->gaki = st->gaki;
                pw_queue(&st->gaki->pw_task, task_file_info_load_file, task);
            }
            if(S_ISDIR(current->stats.st_mode)) {
                 t_panel_file_ensure_exist(&st->t_file_infos, &current->panel_file, current->path);
#if 1

                 if(!current->panel_file->read) {
                     current->panel_file->read = true;

                     Task_Panel_Gaki_Read_Dir *task;
                     NEW(Task_Panel_Gaki_Read_Dir, task);
                     task->gaki = st->gaki;
                     task->path = current->path;
                     task->infos = &current->panel_file->file_infos;
                     task->printable = &current->printable;
                     pw_queue(&st->gaki->pw_task, task_panel_gaki_read_dir, task);
                 }

#else

            //     Task_File_Info_Read_Dir *task;
            //     NEW(Task_File_Info_Read_Dir, task);
            //     task->current = current;
            //     task->gaki = st->gaki;
            //     pw_queue(&st->gaki->pw_task, task_panel_file_read_dir, task);

#endif
            }
        }
    }

    pthread_mutex_unlock(&st->rwlock);
}


void panel_gaki_render(Tui_Buffer *buffer, Panel_Gaki *st) {
    
    So *tmp = &st->tmp;
    
    /* draw file preview */
    so_clear(tmp);
    File_Info *current = 0;
#if 1
    for(size_t i = 0; st->panel_file && i < file_infos_length(st->panel_file->file_infos); ++i) {
        File_Info *unsel = file_infos_get_at(&st->panel_file->file_infos, i);
        unsel->selected = false;
    }

    if(st->panel_file && st->panel_file->select < file_infos_length(st->panel_file->file_infos)) {

        if(!pthread_mutex_lock(&st->rwlock)) {

            current = file_infos_get_at(&st->panel_file->file_infos, st->panel_file->select);
            current->selected = true;
            if(current->printable) {

                if(S_ISREG(current->stats.st_mode)) {

                    tui_buffer_draw(buffer, st->layout.rc_preview, 0, 0, 0, current->content);
                }

                if(S_ISDIR(current->stats.st_mode)) {
                    panel_file_render(buffer, st->layout.rc_preview, current->panel_file);
                }
            }

            pthread_mutex_unlock(&st->rwlock);
        }
    }

#endif

#if 1
    /* draw vertical bar */
    so_clear(tmp);
    for(size_t i = 0; i < st->layout.rc_split.dim.y; ++i) {
        so_extend(tmp, so("│\n"));
    }
    tui_buffer_draw(buffer, st->layout.rc_split, 0, 0, 0, *tmp);

    /* draw file infos */
    panel_file_render(buffer, st->layout.rc_files, st->panel_file);
#endif

#if 1
    /* draw current dir/file/type */
    Tui_Color bar_bg = { .type = TUI_COLOR_8, .col8 = 1 };
    Tui_Color bar_fg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Fx bar_fx = { .bold = true };
    if(current) {

        // so_clear(tmp);
        // so_fmt(tmp, "%.*s (frame %zu)", SO_F(current->path), st->gaki->frames);
        // tui_buffer_draw(buffer, st->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, *tmp);

        so_clear(tmp);
        Tui_Rect rc_mode = st->layout.rc_pwd;
        if(S_ISDIR(current->stats.st_mode)) {
            so_fmt(tmp, "[DIR]");
            bar_bg.col8 = 4;
        } else if(S_ISREG(current->stats.st_mode)) {
            so_fmt(tmp, "[FILE]");
            bar_bg.col8 = 5;
        } else {
            so_fmt(tmp, "[?]");
        }
        rc_mode.dim.x = tmp->len;
        rc_mode.anc.x = buffer->dimension.x - rc_mode.dim.x;
        tui_buffer_draw(buffer, st->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, current->path);
        tui_buffer_draw(buffer, rc_mode, &bar_fg, &bar_bg, &bar_fx, *tmp);

    } else {
        tui_buffer_draw(buffer, st->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, st->pwd);
    }
#endif

}


