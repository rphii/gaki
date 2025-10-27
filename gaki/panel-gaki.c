#include <math.h>
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

void panel_gaki_layout_get_ratio_widths(Panel_Gaki_Config *config, unsigned int *w_files, unsigned int *w_parent, unsigned int *w_preview) {
    ssize_t width = config->rc.dim.x;
    double r_total = config->ratio_files + config->ratio_parent + config->ratio_preview;
    if(r_total) {
        *w_parent = round((double)(width * config->ratio_parent) / r_total);
        *w_files = round((double)(width * config->ratio_files) / r_total);
        *w_preview = round((double)(width * config->ratio_preview) / r_total);
    } else {
        r_total = 7;
        *w_parent = round((double)(width * 1) / r_total);
        *w_files = round((double)(width * 3) / r_total);
        *w_preview = round((double)(width * 3) / r_total);
    }
}

void panel_gaki_layout_from_rules(Panel_Gaki_Layout *layout, Panel_Gaki_Config *config) {

    layout->rc_pwd = config->rc;
    layout->rc_files = config->rc;
    layout->rc_parent = config->rc;
    layout->rc_preview = config->rc;
    layout->rc_split_parent = config->rc;
    layout->rc_split_preview = config->rc;

    unsigned int w_files, w_parent, w_preview, h_bar = 1;
    panel_gaki_layout_get_ratio_widths(config, &w_files, &w_parent, &w_preview);

    /* make space for bar */
    layout->rc_pwd.dim.y = h_bar;
    layout->rc_files.dim.y -= h_bar;
    layout->rc_files.anc.y += h_bar;
    layout->rc_parent.dim.y -= h_bar;
    layout->rc_parent.anc.y += h_bar;
    layout->rc_preview.dim.y -= h_bar;
    layout->rc_preview.anc.y += h_bar;
    layout->rc_split_preview.anc.y += h_bar;
    layout->rc_split_preview.dim.y -= h_bar;
    layout->rc_split_parent.anc.y += h_bar;
    layout->rc_split_parent.dim.y -= h_bar;

    /* parent pane */
    if(w_parent) {
        layout->rc_split_parent.anc.x = w_parent - 1;
        layout->rc_split_parent.dim.x = 1;
        layout->rc_parent.anc.x = 1;
        layout->rc_parent.dim.x = w_parent - 1;
    } else {
        layout->rc_parent.dim.x = 0;
    }

    /* files pane */
    layout->rc_files.anc.x = w_parent;
    layout->rc_files.dim.x = w_files;

    /* preview pane */
    if(w_preview) {
        layout->rc_split_preview.anc.x = w_parent + w_files;
        layout->rc_split_preview.dim.x = 1;
        layout->rc_preview.anc.x = w_parent + w_files + 1;
        layout->rc_preview.dim.x = w_preview - 1;
    } else {
        layout->rc_preview.dim.x = 0;
    }
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

    /* read parent directory */
    if(nav && !nav->parent) {
        So path = so_clone(so_rsplit_ch(nav->pwd.ref->path, PLATFORM_CH_SUBDIR, 0));
        if(!so_len(path)) so_copy(&path, so(PLATFORM_S_SUBDIR));
        if(so_len(path) < so_len(nav->pwd.ref->path)) {
            File_Info *info = file_info_ensure(sync_t, path);
            NEW(Nav_Directory, nav->parent);
            nav->parent->pwd.ref = info;
            nav_directory_dispatch_readdir(pw, sync_m, sync_t, sync, nav->parent, nav);
        }
    }

    pthread_mutex_unlock(&sync->mtx);
}

bool panel_gaki_input(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Tui_Sync_Input *sync_i, Tui_Sync_Draw *sync_d, Tui_Input *input, bool *flush, bool *quit) {

    pthread_mutex_lock(&sync->mtx);

    Action ac = {0};
    bool any = false;

    Nav_Directory *nav = sync->panel_gaki.nav_directory;

    //usleep(1e4);printff("\rSTATE: %b  %b  %b  %b",input->key.down,input->key.press,input->key.release,input->key.repeat);usleep(1e4);
    if(input->id == INPUT_TEXT && input->text.len == 1) {
        switch(input->text.str[0]) {
            case 'q': ac.quit = true; break;
            case 'j': ac.select_down = 1; break;
            case 'k': ac.select_up = 1; break;
            case 'h': ac.select_left = 1; break;
            case 'l': ac.select_right = 1; break;
            case 'L': ac.tab_next = true; break;
            case 'H': ac.tab_prev = true; break;
            case 't': ac.tab_new = true; break;
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
        if(input->mouse.scroll) {
            if(tui_rect_encloses_point(sync->panel_gaki.layout.rc_files, input->mouse.pos)) {
                if(input->mouse.scroll > 0) {
                    ac.select_down = 1;
                } else if(input->mouse.scroll < 0) {
                    ac.select_up = 1;
                }
            }
        }
        if(nav && input->mouse.l.down) {
            if(tui_rect_encloses_point(sync->panel_gaki.layout.rc_files, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync->panel_gaki.layout.rc_files, input->mouse.pos);
                nav->index = pt.y + nav->offset;
                any = true;
            }
        }
        if(nav && input->mouse.l.press) {
            if(tui_rect_encloses_point(sync->panel_gaki.layout.rc_parent, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync->panel_gaki.layout.rc_parent, input->mouse.pos);
                if(nav->parent) {
                    Nav_Directory *replace = nav->parent;
                    if(pt.y + replace->offset < array_len(replace->list)) {
                        replace->index = pt.y + replace->offset;
                    }
                    sync->panel_gaki.nav_directory = replace;
                    any = true;
                }
            }
            if(tui_rect_encloses_point(sync->panel_gaki.layout.rc_preview, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync->panel_gaki.layout.rc_preview, input->mouse.pos);
                if(nav->index < array_len(nav->list)) {
                    Nav_Directory *replace = array_at(nav->list, nav->index);
                    switch(replace->pwd.ref->stats.st_mode & S_IFMT) {
                        case S_IFDIR: {
                            if(pt.y + replace->offset < array_len(replace->list)) {
                                replace->index = pt.y;
                            }
                            sync->panel_gaki.nav_directory = replace;
                            any = true;
                        }
                        default: break;
                    }
                }
            }
        }

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
                *flush = true;
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
            *array_it(sync->panel_gaki.tabs, sync->panel_gaki.tab_sel) = sync->panel_gaki.nav_directory;
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
            if(sync->panel_gaki.tab_sel == SIZE_MAX) {
                sync->panel_gaki.tab_sel = len - 1;
            } else {
                sync->panel_gaki.tab_sel %= len;
            }
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

    /* draw parent */
    if(nav && nav->parent) {
        panel_gaki_render_nav_dir(buffer, &tmp, nav->parent, panel, panel->layout.rc_parent);
    }

    /* draw vertical splits */
    so_clear(&tmp);
    for(size_t i = 0; i < panel->layout.rc_split_preview.dim.y; ++i) {
        so_extend(&tmp, so("│\n"));
    }
    tui_buffer_draw(buffer, panel->layout.rc_split_preview, 0, 0, 0, tmp);

    so_clear(&tmp);
    for(size_t i = 0; i < panel->layout.rc_split_parent.dim.y; ++i) {
        so_extend(&tmp, so("│\n"));
    }
    tui_buffer_draw(buffer, panel->layout.rc_split_parent, 0, 0, 0, tmp);

exit:
    pthread_mutex_unlock(&sync->mtx);

    so_free(&tmp);

}


