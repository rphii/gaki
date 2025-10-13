#include <ctype.h>
#include "panel-gaki.h"

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
    layout->rc_split.anc.x = layout->rc_files.dim.x;
    layout->rc_split.dim.x = 1;

    /* preview right half */
    layout->rc_preview.anc.x = layout->rc_split.anc.x + 1;
    layout->rc_preview.dim.x = config->rc.dim.x - layout->rc_preview.anc.x;
}

void panel_gaki_update(Panel_Gaki *st, Action *ac) {

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
    }

    if(ac->select_up) {
        panel_gaki_select_up(st, ac->select_up);
    }

    if(ac->select_down) {
        panel_gaki_select_down(st, ac->select_down);
    }

    memset(ac, 0, sizeof(*ac));

}


void panel_gaki_render(Tui_Buffer *buffer, Panel_Gaki *st) {
    
    So *tmp = &st->tmp;
    
    /* draw file preview */
#if 1
    so_clear(tmp);
    File_Info *current = 0;
    for(size_t i = 0; st->panel_file && i < file_infos_length(st->panel_file->file_infos); ++i) {
        File_Info *unsel = file_infos_get_at(&st->panel_file->file_infos, i);
        unsel->selected = false;
    }
    if(st->panel_file && st->panel_file->select < file_infos_length(st->panel_file->file_infos)) {
        current = file_infos_get_at(&st->panel_file->file_infos, st->panel_file->select);
        current->selected = true;
        if(S_ISREG(current->stats.st_mode)) {
            if(!so_len(current->content)) {
                so_file_read(current->path, &current->content);
                current->printable = true;
                for(size_t i = 0; i < so_len(current->content); ++i) {
                    unsigned char c = so_at(current->content, i);
                    if(!(c >= ' ' || isspace(c))) {
                        current->printable = false;
                        break;
                    }
                }
                if(!current->printable) {
                    so_free(&current->content);
                }
            }
            if(current->printable) {
                tui_buffer_draw(buffer, st->layout.rc_preview, 0, 0, 0, current->content);
            }
        } else if(S_ISDIR(current->stats.st_mode)) {
            t_panel_file_ensure_exist(&st->t_file_infos, &current->panel_file, current->path);
            current->printable = true;
            panel_file_render(buffer, st->layout.rc_preview, current->panel_file);
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

    /* draw current dir/file/type */
    Tui_Color bar_bg = { .type = TUI_COLOR_8, .col8 = 1 };
    Tui_Color bar_fg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Fx bar_fx = { .bold = true };
    if(current) {
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
        tui_buffer_draw(buffer, st->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, current->path);
        rc_mode.dim.x = tmp->len;
        rc_mode.anc.x = buffer->dimension.x - rc_mode.dim.x;
        tui_buffer_draw(buffer, rc_mode, &bar_fg, &bar_bg, &bar_fx, *tmp);
    } else {
        tui_buffer_draw(buffer, st->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, st->pwd);
    }

}


