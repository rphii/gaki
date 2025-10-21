#include <ctype.h>
#include <dirent.h>
#include <sys/wait.h>

#include "panel-gaki.h"
#include "gaki.h"

#if 0
void panel_gaki_select_up(Panel_Gaki *panel, size_t n) {
    if(!panel->panel_file) return;
    if(!panel->panel_file->select) {
        panel->panel_file->select = file_infos_length(panel->panel_file->file_infos) - 1;
        panel->panel_file->offset = panel->panel_file->select + 1 > panel->layout.rc_files.dim.y ? panel->panel_file->select + 1 - panel->layout.rc_files.dim.y : 0;
    } else {
        --panel->panel_file->select;
        if(panel->panel_file->offset) {
            --panel->panel_file->offset;
        }
    }
}

void panel_gaki_select_down(Panel_Gaki *panel, size_t n) {
    if(!panel->panel_file) return;
    ++panel->panel_file->select;
    if(panel->panel_file->select >= file_infos_length(panel->panel_file->file_infos)) {
        panel->panel_file->select = 0;
        panel->panel_file->offset = 0;
    }
    if(panel->panel_file->select >= panel->panel_file->offset + panel->layout.rc_files.dim.y) {
        ++panel->panel_file->offset;
    }
}

void panel_gaki_select_at(Panel_Gaki *panel, size_t n) {
}
#endif

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

#if 0
int panel_file_read_sync(T_Panel_File *panels, So path, File_Infos *file_infos) {
    DIR *d;
    struct dirent *dir;
    char *cdirname = so_dup(path);
    char *cpath = 0;
    d = opendir(cdirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            File_Info info = {0};
            so_path_join(&info.path, path, so_l(dir->d_name));
            cpath = so_dup(info.path);
            stat(cpath, &info.stats);
            if(S_ISDIR(info.stats.st_mode)) {
                t_panel_file_ensure_exist(panels, &info.panel_file, info.path);
            }
            file_infos_push_back(file_infos, &info);
        }
        closedir(d);
    }
    free(cdirname);
    free(cpath);
    file_infos_sort(file_infos);
    return(0);
}
#endif


typedef struct Task_File_Info_Load_File {
    File_Info *current;
    Gaki *gaki;
} Task_File_Info_Load_File;

void *task_file_info_load_file(Pw *pw, bool *quit, void *void_task) {
    Task_File_Info_Load_File *task = void_task;

    So content = SO;

    so_file_read(task->current->path, &content);
    bool printable = so_len(content);
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

    //pthread_mutex_lock(&task->gaki->sync_panel.mtx);
    //task->current->content = content;
    //task->current->printable = printable;
    //pthread_mutex_unlock(&task->gaki->sync_panel.mtx);

    if(printable) {
        pthread_mutex_lock(&task->gaki->sync_main.mtx);
        ++task->gaki->sync_main.render_do;
        pthread_cond_signal(&task->gaki->sync_main.cond);
        pthread_mutex_unlock(&task->gaki->sync_main.mtx);
    }

    free(task);
    return 0;
}

typedef struct Task_Panel_Gaki_Read_Dir {
    So path;
    Gaki *gaki;
} Task_Panel_Gaki_Read_Dir;

void *task_panel_gaki_read_dir(Pw *pw, bool *quit, void *void_task) {
    Task_Panel_Gaki_Read_Dir *task = void_task;

#if 0
    Panel_File *panel = 0;
    bool printable = false;

#if 0
    t_panel_file_ensure_exist(&task->gaki->t_panels, &panel, task->path);
    if(!panel->have_read) {
        panel->have_read = true;
        panel_file_read_sync(&task->gaki->t_panels, task->path, &panel->file_infos);
        printable = file_infos_length(panel->file_infos);
    }
#endif

    pthread_mutex_lock(&task->gaki->panel_gaki.rwlock);
    pthread_mutex_unlock(&task->gaki->panel_gaki.rwlock);

    if(printable) {
        pthread_mutex_lock(&task->gaki->sync_main.mtx);
        ++task->gaki->sync_main.update_do;
        pthread_cond_signal(&task->gaki->sync_main.cond);
        pthread_mutex_unlock(&task->gaki->sync_main.mtx);
    }
#endif

    free(task);
    return 0;
}


typedef struct Task_File_Info_Read_Dir {
    File_Info *current;
    Gaki *gaki;
} Task_File_Info_Read_Dir;

void *task_panel_file_read_dir(Pw *pw, bool *quit, void *void_task) {
    Task_File_Info_Read_Dir *task = void_task;

    //Panel_File panel = {0};
#if 0
    panel_file_read_sync(&task->gaki->t_panels, task->current->path, &panel.file_infos);

    pthread_mutex_lock(&task->gaki->panel_gaki.rwlock);
    *task->current->panel_file = panel;
    task->current->printable = true;
    pthread_mutex_unlock(&task->gaki->panel_gaki.rwlock);
#endif

    pthread_mutex_lock(&task->gaki->sync_main.mtx);
    ++task->gaki->sync_main.update_do;
    pthread_cond_signal(&task->gaki->sync_main.cond);
    pthread_mutex_unlock(&task->gaki->sync_main.mtx);

    free(task);
    return 0;
}


void panel_gaki_update(Gaki *gaki, Panel_Gaki *panel, Action *ac) {

#if 0
    pthread_mutex_lock(&gaki->sync_panel.mtx);

    panel_gaki_layout_from_rules(&panel->layout, &panel->config);

    if(ac->select_left) {
        So left = so_ensure_dir(so_rsplit_ch(panel->path, PLATFORM_CH_SUBDIR, 0));
        if(left.len) {
            panel->path = left;
            panel->panel_file = 0;
        } else {
            so_copy(&panel->path, so("/"));
            panel->panel_file = 0;
        }
    }

    if(ac->select_right && panel->panel_file) {
        if(panel->panel_file->select < file_infos_length(panel->panel_file->file_infos)) {
            File_Info *sel = file_infos_get_at(&panel->panel_file->file_infos, panel->panel_file->select);
            if(S_ISDIR(sel->stats.st_mode)) {
                so_path_join(&panel->path, panel->path, so_get_basename(sel->path));
                panel->panel_file = 0;

            } else if(S_ISREG(sel->stats.st_mode)) {

                So ed = SO;
                so_env_get(&ed, so("EDITOR"));
                if(!so_len(ed)) {
                    // TODO make some kind of notice
                    printff("\rNO EDITOR FOUND");
                    exit(1);
                }

                /* pause input */
                pthread_mutex_lock(&gaki->sync_input.mtx);
                gaki->sync_input.idle = true;
                pthread_mutex_unlock(&gaki->sync_input.mtx);

                pid_t pid = fork();
                if(pid < 0) {
                    // TODO make some kind of notice
                    printff("\rFORK FAILED");
                    exit(1);
                } else if(!pid) {
                    char *ced = so_dup(ed);
                    char *cpath = so_dup(sel->path);
                    char *cargs[] = { ced, cpath, 0 };
                    system("tput rmcup");
                    execvp(ced, cargs);
                    _exit(EXIT_FAILURE);
                }

                int status;
                waitpid(pid, &status, 0);

                if(status) {
                    // TODO make some kind of notice
                    printff("\rCHILD FAILED");
                    exit(1);
                }

                system("tput smcup");

                /* resume input */
                pthread_mutex_lock(&gaki->sync_input.mtx);
                gaki->sync_input.idle = false;
                pthread_cond_signal(&gaki->sync_input.cond);
                pthread_mutex_unlock(&gaki->sync_input.mtx);

                /* issue a redraw */
                pthread_mutex_lock(&gaki->sync_draw.mtx);
                ++gaki->sync_draw.draw_redraw;
                pthread_mutex_unlock(&gaki->sync_draw.mtx);

                /* force update */
                pthread_mutex_lock(&gaki->sync_main.mtx);
                ++gaki->sync_main.update_do;
                pthread_cond_signal(&gaki->sync_main.cond);
                pthread_mutex_unlock(&gaki->sync_main.mtx);

#if 0
                    so_fmt(&ed, " '%.*s'", SO_F(sel->path));
                    ced = so_dup(ed);
                    tui_exit();
                    system(ced);
                    tui_enter();
#endif
                so_free(&ed);
            }
        }
    }

    if(!so_len(panel->path)) {
        so_env_get(&panel->path, so("PWD"));
    }

    if(!panel->panel_file) {
#if 0
        t_panel_file_ensure_exist(&gaki->t_panels, &panel->panel_file, panel->path);

        if(!panel->panel_file->have_read) {
            panel->panel_file->have_read = true;

            Task_Panel_Gaki_Read_Dir *task;
            NEW(Task_Panel_Gaki_Read_Dir, task);
            task->gaki = gaki;
            task->path = panel->path;
            pw_queue(&gaki->pw_task, task_panel_gaki_read_dir, task);
        }
#endif
    }

    if(ac->select_up) {
        panel_gaki_select_up(panel, ac->select_up);
    }

    if(ac->select_down) {
        panel_gaki_select_down(panel, ac->select_down);
    }

    if(panel->panel_file && panel->panel_file->select < file_infos_length(panel->panel_file->file_infos)) {
        File_Info *current = file_infos_get_at(&panel->panel_file->file_infos, panel->panel_file->select);
#if 0
        if(!current->have_read) {
            current->have_read = true;
            if(S_ISREG(current->stats.st_mode)) {
                Task_File_Info_Load_File *task;
                NEW(Task_File_Info_Load_File, task);
                task->current = current;
                task->gaki = gaki;
                pw_queue(&gaki->pw_task, task_file_info_load_file, task);
            }
            if(S_ISDIR(current->stats.st_mode)) {
                 t_panel_file_ensure_exist(&gaki->t_panels, &current->panel_file, current->path);

                 if(!current->panel_file->have_read) {
                     current->panel_file->have_read = true;

                     Task_Panel_Gaki_Read_Dir *task;
                     NEW(Task_Panel_Gaki_Read_Dir, task);
                     task->gaki = gaki;
                     task->path = current->path;
                     //task->infos = &current->panel_file->file_infos;
                     //task->printable = &current->printable;
                     pw_queue(&gaki->pw_task, task_panel_gaki_read_dir, task);
                 }
            }
        }
#endif
    }

    pthread_mutex_unlock(&gaki->sync_panel.mtx);
#endif
}


void panel_gaki_render(Tui_Buffer *buffer, Gaki_Sync_Panel *sync) {
    
    So tmp = SO;
    
    /* draw file preview */
    so_clear(&tmp);

#if 1

    Panel_Gaki *panel = &sync->panel_gaki;
    Tui_Rect dim = { .dim = buffer->dimension };
    dim.dim.y = 1;

    pthread_mutex_lock(&sync->mtx);
    if(!panel->nav_directory) goto exit;

    Nav_Directory *nav = sync->panel_gaki.nav_directory;
    if(!nav) goto exit;

    Nav_File_Info *pwd = &nav->pwd;
    if(!pwd->ref) goto exit;

    tui_buffer_draw(buffer, dim, 0, 0, 0, pwd->ref->path);

    for(size_t i = 0; i < array_len(nav->list); ++i) {
        Nav_Directory *nav_sub = array_at(nav->list, i);
        ++dim.anc.y;
        so_clear(&tmp);
        so_fmt(&tmp, "%.*s", SO_F(so_get_basename(nav_sub->pwd.ref->path)));
        tui_buffer_draw(buffer, dim, 0, 0, 0, tmp);
    }

exit:
    pthread_mutex_unlock(&sync->mtx);

#else
#if 0
    Nav_Directory *dir = panel->nav_directory;

    size_t len = array_len(dir->list);

    for(size_t i = 0; i < len; ++i) {
        Nav_File_Info *unsel = array_at(dir->list, i);
        unsel->selected = false;
    }

    if(dir->index < len) {

        if(!pthread_mutex_lock(&gaki->sync_panel.mtx)) {

            current = array_at(panel->nav_directory->list, panel->nav_directory->index);
            current->pwd.selected = true;
            if(current->pwd.printable) {

                if(S_ISREG(current->pwd.ref->stats.st_mode)) {
                    tui_buffer_draw(buffer, panel->layout.rc_preview, 0, 0, 0, current->pwd.ref->content.text);
                }

                if(S_ISDIR(current->pwd.ref->stats.st_mode)) {
                    //panel_directory_render(buffer, panel->layout.rc_preview, current->ref->content.files);
                }
            }

            pthread_mutex_unlock(&gaki->sync_panel.mtx);
        }
    }

#endif

#if 1
    /* draw vertical bar */
    so_clear(&tmp);
    for(size_t i = 0; i < panel->layout.rc_split.dim.y; ++i) {
        so_extend(&tmp, so("│\n"));
    }
    tui_buffer_draw(buffer, panel->layout.rc_split, 0, 0, 0, tmp);

    /* draw file infos */
    //panel_directory_render(buffer, panel->layout.rc_files, panel->directory);
#endif

#if 1
    /* draw current dir/file/type */
    Tui_Color bar_bg = { .type = TUI_COLOR_8, .col8 = 1 };
    Tui_Color bar_fg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Fx bar_fx = { .bold = true };
    if(current) {

        // so_clear(tmp);
        // so_fmt(tmp, "%.*s (frame %zu)", SO_F(current->path), panel->gaki->frames);
        // tui_buffer_draw(buffer, panel->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, *tmp);

#if 0
        so_clear(&tmp);
        Tui_Rect rc_mode = panel->layout.rc_pwd;
        if(S_ISDIR(current->ref->stats.st_mode)) {
            so_fmt(&tmp, "[DIR]");
            bar_bg.col8 = 4;
        } else if(S_ISREG(current->ref->stats.st_mode)) {
            so_fmt(&tmp, "[FILE]");
            bar_bg.col8 = 5;
        } else {
            so_fmt(&tmp, "[?]");
        }
        rc_mode.dim.x = tmp.len;
        rc_mode.anc.x = buffer->dimension.x - rc_mode.dim.x;
        tui_buffer_draw(buffer, panel->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, current->ref->path);
        tui_buffer_draw(buffer, rc_mode, &bar_fg, &bar_bg, &bar_fx, tmp);
#endif

    } else {
        //tui_buffer_draw(buffer, panel->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, panel->path);
    }
#endif
#endif

    so_free(&tmp);

}


