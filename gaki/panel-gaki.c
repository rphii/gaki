#include <ctype.h>
#include <dirent.h>
#include <sys/wait.h>

#include "panel-gaki.h"
#include "gaki.h"

void panel_gaki_select_up(Panel_Gaki *panel, size_t n) {
    if(!panel->nav_directory) return;
    if(!panel->nav_directory->index) {
        panel->nav_directory->index = array_len(panel->nav_directory->list) - 1;
        panel->nav_directory->offset = panel->nav_directory->index + 1 > panel->layout.rc_files.dim.y ? panel->nav_directory->index + 1 - panel->layout.rc_files.dim.y : 0;
    } else {
        --panel->nav_directory->index;
        if(panel->nav_directory->offset) {
            --panel->nav_directory->offset;
        }
    }
}

void panel_gaki_select_down(Panel_Gaki *panel, size_t n) {
    if(!panel->nav_directory) return;
    ++panel->nav_directory->index;
    if(panel->nav_directory->index >= array_len(panel->nav_directory->list)) {
        panel->nav_directory->index = 0;
        panel->nav_directory->offset = 0;
    }
    if(panel->nav_directory->index >= panel->nav_directory->offset + panel->layout.rc_files.dim.y) {
        ++panel->nav_directory->offset;
    }
}

void panel_gaki_select_at(Panel_Gaki *panel, size_t n) {
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


void panel_gaki_update(Pw *pw, Gaki_Sync_Panel *sync, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t) {

    pthread_mutex_lock(&sync->mtx);

    panel_gaki_layout_from_rules(&sync->panel_gaki.layout, &sync->panel_gaki.config);

    Nav_Directory *nav = sync->panel_gaki.nav_directory;
    if(nav && nav->index < array_len(nav->list)) {
        Nav_Directory *nav_sub = array_at(nav->list, nav->index);
        if(nav_sub && nav_sub->pwd.ref) {
            nav_directory_dispatch_readany(pw, sync_m, sync_t, sync, nav_sub);
        }
    }

    if(!nav->parent) {
        So path = so_clone(so_rsplit_ch(nav->pwd.ref->path, PLATFORM_CH_SUBDIR, 0));
        if(!so_len(path)) so_copy(&path, so(PLATFORM_S_SUBDIR));
        if(so_len(path) < so_len(nav->pwd.ref->path)) {
#if 0
            printff("\r[%.*s] -> PARENT:[%.*s]",SO_F(nav->pwd.ref->path),SO_F(path));sleep(1);
            File_Info *info = file_info_ensure(sync_t, path);
            NEW(Nav_Directory, nav->parent);
            nav->parent->pwd.ref = info;
            nav_directory_dispatch_readdir(pw, sync_m, sync_t, sync, nav->parent);
#else
            File_Info *info = file_info_ensure(sync_t, path);
            NEW(Nav_Directory, nav->parent);
            nav->parent->pwd.ref = info;

            nav_directory_dispatch_readdir(pw, sync_m, sync_t, sync, nav->parent, nav);
#endif
        }
    }

    pthread_mutex_unlock(&sync->mtx);
}

bool panel_gaki_input(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Tui_Sync_Input *sync_i, Tui_Sync_Draw *sync_d, Tui_Input *input, bool *quit) {

    pthread_mutex_lock(&sync->mtx);

    Action ac = {0};
    bool any = false;

    if(input->id == INPUT_TEXT && input->text.len == 1) {
        switch(input->text.str[0]) {
            case 'q': ac.quit = true; break;
            case 'j': ac.select_down = 1; break;
            case 'k': ac.select_up = 1; break;
            case 'h': ac.select_left = 1; break;
            case 'l': ac.select_right = 1; break;
            case 'L': ac.tab_next = true; break;
            case 'H': ac.tab_prev = true; break;
            case 'K': ac.tab_new = true; break;
            //case '/': gaki->ac. = 1; break;
            default: break;
        }
    }

    if(input->id == INPUT_CODE) {
        if(input->code == KEY_CODE_UP) {
            ac.select_up = 1;
        }
        if(input->code == KEY_CODE_DOWN) {
            ac.select_down = 1;
        }
        if(input->code == KEY_CODE_LEFT) {
            ac.select_left = 1;
        }
        if(input->code == KEY_CODE_RIGHT) {
            ac.select_right = 1;
        }
    }

    if(input->id == INPUT_MOUSE) {
#if 0
        if(tui_rect_encloses_point(sync_panel.panel_gaki.layout.rc_files, input->mouse.pos)) {
            if(input->mouse.scroll > 0) {
                ac.select_down = 1;
            } else if(input->mouse.scroll < 0) {
                ac.select_up = 1;
            }
        }
        if(input->mouse.l) {
            if(tui_rect_encloses_point(sync_panel.panel_gaki.layout.rc_files, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync_panel.panel_gaki.layout.rc_files, input->mouse.pos);
                if(sync_panel.panel_gaki.nav_directory) {
                    sync_panel.panel_gaki.nav_directory->index = pt.y + sync_panel.panel_gaki.nav_directory->offset;
                }
            }
        }
#endif
        // if(input->mouse.m) {
        // }
        // if(input->mouse.r) {
        //     if(!input_prev.mouse.r) {
        //     }
        //     if(resize_split) {
        //     }
        // } else {
        //     resize_split = false;
        // }
    }

    if(ac.select_up) {
        panel_gaki_select_up(&sync->panel_gaki, ac.select_up);
        any = true;
    }

    if(ac.select_down) {
        panel_gaki_select_down(&sync->panel_gaki, ac.select_down);
        any = true;
    }

    if(ac.select_right) {
        Nav_Directory *nav = sync->panel_gaki.nav_directory;
        if(nav && nav->index < array_len(nav->list)) {
            nav = array_at(nav->list, nav->index);
        }
        switch(nav->pwd.ref->stats.st_mode & S_IFMT) {
            case S_IFDIR: {
                sync->panel_gaki.nav_directory = nav;
                any = true;
            } break;
            case S_IFREG: {

#if 1
                So ed = SO;
                so_env_get(&ed, so("EDITOR"));
                if(!so_len(ed)) {
                    // TODO make some kind of notice
                    printff("\rNO EDITOR FOUND");
                    exit(1);
                }

                /* pause input */
                tui_sync_input_idle(sync_i);

                pid_t pid = fork();
                if(pid < 0) {
                    // TODO make some kind of notice
                    printff("\rFORK FAILED");
                    exit(1);
                } else if(!pid) {
                    char *ced = so_dup(ed);
                    char *cpath = so_dup(nav->pwd.ref->path);
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
                tui_sync_input_wake(sync_i);

                /* issue a redraw */
                tui_sync_redraw(sync_d);
#endif

            } break;
            default: break;
        }
    }

    if(ac.tab_new) {
        Nav_Directory *nav = sync->panel_gaki.nav_directory;
        if(nav->pwd.ref) {
            nav_directory_dispatch_register(pw, sync_m, sync_t, sync, nav->pwd.ref->path);
        }
        any = true;
    }

    if(ac.tab_next) {
        size_t len = array_len(sync->panel_gaki.tabs);
        if(len > 1) {
            *array_it(sync->panel_gaki.tabs, sync->panel_gaki.tab_sel) = sync->panel_gaki.nav_directory;
            ++sync->panel_gaki.tab_sel;
            sync->panel_gaki.tab_sel %= len;
            sync->panel_gaki.nav_directory = array_at(sync->panel_gaki.tabs, sync->panel_gaki.tab_sel);
            any = true;
        }
    }

    if(ac.tab_prev) {
        size_t len = array_len(sync->panel_gaki.tabs);
        if(len > 1) {
            *array_it(sync->panel_gaki.tabs, sync->panel_gaki.tab_sel) = sync->panel_gaki.nav_directory;
            --sync->panel_gaki.tab_sel;
            sync->panel_gaki.tab_sel %= len;
            sync->panel_gaki.nav_directory = array_at(sync->panel_gaki.tabs, sync->panel_gaki.tab_sel);
            any = true;
        }
    }

    if(ac.select_left) {
        Nav_Directory *nav = sync->panel_gaki.nav_directory;
        if(nav && nav->parent) {
            sync->panel_gaki.nav_directory = nav->parent;
            any = true;
        }
    }

    pthread_mutex_unlock(&sync->mtx);

    *quit = ac.quit;
    return any;
}

void panel_gaki_render_nav_dir(Tui_Buffer *buffer, So *tmp, Nav_Directory *nav, Panel_Gaki *panel, Tui_Rect rc) {

    /* print file list */
    Tui_Color dir_fg = { .type = TUI_COLOR_8, .col8 = 0 };
    Tui_Color dir_bg = { .type = TUI_COLOR_8, .col8 = 7 };
    rc.dim.y = 1;
    size_t list_len = array_len(nav->list);
    //printff("RENDER: [%.*s] LEN %zu", SO_F(nav->pwd.ref->path), list_len);
    for(size_t i = nav->offset; i < list_len; ++i) {
        if(rc.anc.y >= buffer->dimension.y) break;
        Nav_Directory *nav_sub = array_at(nav->list, i);
        Tui_Color *fg = (nav->index == i) ? &dir_fg : 0;
        Tui_Color *bg = (nav->index == i) ? &dir_bg : 0;
        so_clear(tmp);
        so_fmt(tmp, "%.*s", SO_F(so_get_nodir(nav_sub->pwd.ref->path)));
        tui_buffer_draw(buffer, rc, fg, bg, 0, *tmp);
        ++rc.anc.y;
    }
}

void panel_gaki_render(Tui_Buffer *buffer, Gaki_Sync_Panel *sync) {
    
    So tmp = SO;
    
    /* draw file preview */
    so_clear(&tmp);

    Panel_Gaki *panel = &sync->panel_gaki;

    pthread_mutex_lock(&sync->mtx);
    if(!panel->nav_directory) goto exit;

    Nav_Directory *nav = sync->panel_gaki.nav_directory;
    if(!nav) goto exit;

    Nav_File_Info *pwd = &nav->pwd;
    if(!pwd->ref) goto exit;

    panel_gaki_render_nav_dir(buffer, &tmp, nav, panel, panel->layout.rc_files);

    /* draw current dir/file/type */
    Tui_Color bar_bg = { .type = TUI_COLOR_8, .col8 = 1 };
    Tui_Color bar_fg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Fx bar_fx = { .bold = true };
    Nav_Directory *current = nav->index < array_len(nav->list) ? array_at(nav->list, nav->index) : 0;
    size_t tab_len = array_len(panel->tabs);
    if(current && nav->pwd.ref) {

        switch(current->pwd.ref->stats.st_mode & S_IFMT) {
            case S_IFDIR: { bar_bg.col8 = 4; } break;
            case S_IFREG: { bar_bg.col8 = 5; } break;
            default: break;
        }

        so_clear(&tmp);
        so_fmt(&tmp, "[%zu/%zu] %.*s", panel->tab_sel + 1, tab_len, SO_F(current->pwd.ref->path));
        tui_buffer_draw(buffer, panel->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, tmp);

        so_clear(&tmp);
        Tui_Rect rc_mode = panel->layout.rc_pwd;
        switch(current->pwd.ref->stats.st_mode & S_IFMT) {
            case S_IFDIR: { so_fmt(&tmp, "[DIR]"); } break;
            case S_IFREG: { so_fmt(&tmp, "[FILE]"); } break;
            default: { so_fmt(&tmp, "[?]"); } break;
        }
        rc_mode.dim.x = tmp.len;
        rc_mode.anc.x = buffer->dimension.x - rc_mode.dim.x;
        //tui_buffer_draw(buffer, panel->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, current->pwd.ref->path);
        tui_buffer_draw(buffer, rc_mode, &bar_fg, &bar_bg, &bar_fx, tmp);

    } else {
        tui_buffer_draw(buffer, panel->layout.rc_pwd, &bar_fg, &bar_bg, &bar_fx, nav->pwd.ref->path);
    }

    /* draw content */
    if(current && current->pwd.ref->loaded) {
        //tui_buffer_draw(buffer, panel->layout.rc_preview, &bar_fg, &bar_bg, &bar_fx, so("HAVE READ"));
        switch(current->pwd.ref->stats.st_mode & S_IFMT) {
            case S_IFREG: {
                //printff("\r[%.*s]",SO_F(current->pwd.ref->content.text));
                tui_buffer_draw(buffer, panel->layout.rc_preview, 0, 0, 0, current->pwd.ref->content.text);
            } break;
            case S_IFDIR: {
                panel_gaki_render_nav_dir(buffer, &tmp, current, panel, panel->layout.rc_preview);
            } break;
        }
    }

    /* draw vertical split */
    so_clear(&tmp);
    for(size_t i = 0; i < panel->layout.rc_split.dim.y; ++i) {
        so_extend(&tmp, so("│\n"));
    }
    tui_buffer_draw(buffer, panel->layout.rc_split, 0, 0, 0, tmp);

exit:
    pthread_mutex_unlock(&sync->mtx);

    so_free(&tmp);

}


