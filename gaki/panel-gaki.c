#include <math.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/wait.h>

#include "panel-gaki.h"
#include "gaki.h"

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

void nav_directory_layout_from_rules(Nav_Directory_Layout *layout, Tui_Rect rc, Nav_Directory *nav, Panel_Input *panel_i) {
    ASSERT_ARG(layout);
    if(!nav) return;

    layout->rc = rc;

    /* filter */
    if(nav->filter.visual_len || &layout->rc_filter == panel_i->config.rc) {
        --layout->rc.dim.y;
        layout->rc_filter = rc;
        layout->rc_filter.dim.y = 1;
        layout->rc_filter.anc.y += layout->rc.dim.y;
    } else {
        layout->rc_filter = (Tui_Rect){0};
    }

    /* search */
    if(nav->search.visual_len || &layout->rc_search == panel_i->config.rc) {
        --layout->rc.dim.y;
        layout->rc_search = rc;
        layout->rc_search.dim.y = 1;
        layout->rc_search.anc.y += layout->rc.dim.y;
    } else {
        layout->rc_search = (Tui_Rect){0};
    }
}

void panel_gaki_layout_from_rules(Panel_Gaki_Layout *layout, Panel_Gaki_Config *config, Nav_Directory *nav, Panel_Input *panel_i) {

    Tui_Rect rc_files, rc_parent, rc_preview;

    layout->rc_pwd = config->rc;
    rc_files = config->rc;
    rc_parent = config->rc;
    rc_preview = config->rc;
    layout->rc_split_parent = config->rc;
    layout->rc_split_preview = config->rc;

    unsigned int w_files, w_parent, w_preview, h_bar = 1;
    panel_gaki_layout_get_ratio_widths(config, &w_files, &w_parent, &w_preview);

    /* make space for bar */
    layout->rc_pwd.dim.y = h_bar;
    rc_files.dim.y -= h_bar;
    rc_files.anc.y += h_bar;
    rc_parent.dim.y -= h_bar;
    rc_parent.anc.y += h_bar;
    rc_preview.dim.y -= h_bar;
    rc_preview.anc.y += h_bar;
    layout->rc_split_preview.anc.y += h_bar;
    layout->rc_split_preview.dim.y -= h_bar;
    layout->rc_split_parent.anc.y += h_bar;
    layout->rc_split_parent.dim.y -= h_bar;

    /* parent pane */
    if(w_parent) {
        layout->rc_split_parent.anc.x = w_parent - 1;
        layout->rc_split_parent.dim.x = 1;
        rc_parent.anc.x = 0;
        rc_parent.dim.x = w_parent - 1;
    } else {
        rc_parent.dim.x = 0;
    }

    /* files pane */
    rc_files.anc.x = w_parent;
    rc_files.dim.x = w_files;

    /* preview pane */
    if(w_preview) {
        layout->rc_split_preview.anc.x = w_parent + w_files;
        layout->rc_split_preview.dim.x = 1;
        rc_preview.anc.x = w_parent + w_files + 1;
        rc_preview.dim.x = config->rc.dim.x - rc_preview.anc.x;
    } else {
        rc_preview.dim.x = 0;
    }

    nav_directory_layout_from_rules(&layout->files, rc_files, nav, panel_i);
    if(nav) {
        nav_directory_layout_from_rules(&layout->parent, rc_parent, nav->parent, panel_i);
        if(nav->index < array_len(nav->list)) {
            nav_directory_layout_from_rules(&layout->preview, rc_preview, array_at(nav->list, nav->index), panel_i);
        }
    }
}


void panel_gaki_update(Gaki_Sync_Panel *sync, Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Panel_Input *panel_i, double ratio_cell_xy) {

    pthread_mutex_lock(&sync->mtx);

    panel_gaki_layout_from_rules(&sync->panel_gaki.layout, &sync->panel_gaki.config, sync->panel_gaki.nav_directory, panel_i);

    /* if invalid file selected */
    while(sync->panel_gaki.nav_directory) {
        Nav_Directory *nav = sync->panel_gaki.nav_directory;
        if(!nav) break;
        if(!nav->pwd.ref) break;
        if(!nav->pwd.have_read) break;
        if(nav->pwd.ref->exists && !S_ISREG(nav->pwd.ref->stats.st_mode)) break;
        if(!nav->parent) break;
        sync->panel_gaki.nav_directory = nav->parent;
    }

    Nav_Directory *nav = sync->panel_gaki.nav_directory;
    for(size_t i = 0; nav && i < array_len(nav->list); ++i) {
        Nav_Directory *nav_sub = array_at(nav->list, i);
        if(nav_sub && nav_sub->pwd.ref) {
            nav_directory_dispatch_readany(pw, sync_m, sync_t, sync, nav_sub);
        }
    }

    if(nav && nav->index < array_len(nav->list)) {
        Nav_Directory *current = array_at(nav->list, nav->index);
        pthread_mutex_lock(&current->pwd.mtx);
        if(current->pwd.ref->signature_id == SO_FILESIG_PNG ||
           current->pwd.ref->signature_id == SO_FILESIG_JPEG) {
            task_file_info_image_cvt_dispatch(pw, current->pwd.ref, sync->panel_gaki.layout.preview.rc.dim, sync_m, ratio_cell_xy);
        }
        pthread_mutex_unlock(&current->pwd.mtx);
    }
    //if(nav && nav->pwd.ref) {
    //    usleep(1e5);printff("\rnav->pwd.ref: %p, have_read %u, exists %u", nav->pwd.ref, nav->pwd.have_read, nav->pwd.ref->exists);usleep(1e6);
    //    if(nav->pwd.have_read && !nav->pwd.ref->exists) {
    //        sync->panel_gaki.nav_directory = nav->parent;
    //    }
    //}

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
    
    /* make sure we still have something selected even if filtering */
    nav_directory_select_any_next_visible(nav);

    /* put all indices into frame */
    nav_directory_offset_center(nav, sync->panel_gaki.layout.files.rc.dim);
    if(nav && nav->parent) {
        nav_directory_offset_center(nav->parent, sync->panel_gaki.layout.files.rc.dim);
    }
    if(nav && nav->index < array_len(nav->list)) {
        nav_directory_offset_center(array_at(nav->list, nav->index), sync->panel_gaki.layout.files.rc.dim);
    }

    if(panel_i->visible && (panel_i->config.rc == &sync->panel_gaki.layout.files.rc_search)) {
        if(nav && so_len(nav->search.so)) {
            nav_directory_search_next(nav, nav->index, nav->search.so);
        }
    }

    pthread_mutex_unlock(&sync->mtx);
}

bool panel_gaki_input(Gaki_Sync_Panel *sync, Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Tui_Sync_Input *sync_i, Tui_Sync_Draw *sync_d, Gaki_Config *cfg, Tui_Input *input, Panel_Input *panel_i, bool *flush, bool *quit) {

    pthread_mutex_lock(&sync->mtx);

    Action ac = {0};
    bool any = false;

    Nav_Directory *nav = sync->panel_gaki.nav_directory;

    if(input->id == INPUT_TEXT) {
        switch(input->text.val) {
            case 'q': ac.quit = true; break;
            case 'j': ac.select_down = 1; break;
            case 'k': ac.select_up = 1; break;
            case 'h': ac.select_left = 1; break;
            case 'l': ac.select_right = 1; break;
            case 'L': ac.tab_next = true; break;
            case 'H': ac.tab_prev = true; break;
            case 't': ac.tab_new = true; break;
            case 'f': ac.filter = true; break;
            case 'F': ac.filter_clear = true; break;
            case '/': ac.search = true; break;
            case '?': ac.search_clear = true; break;
            case 'n': ac.search_next = true; break;
            case 'N': ac.search_prev = true; break;
            case 'v': ac.select_toggle = true; break;
            //case '/': gaki->ac. = 1; break;
            default: break;
        }
    }

    if(ac.filter || ac.filter_clear) {
        any = true;
        panel_i->mtx = &sync->mtx;
        panel_i->text = &nav->filter;
        panel_i->visible = true;
        panel_i->config.rc = &sync->panel_gaki.layout.files.rc_filter;
        panel_i->config.prompt =  cfg->filter_prefix;
        if(ac.filter_clear) tui_text_line_clear(panel_i->text);
    }

    if(ac.search || ac.search_clear) {
        any = true;
        panel_i->mtx = &sync->mtx;
        panel_i->text = &nav->search;
        panel_i->visible = true;
        panel_i->config.rc = &sync->panel_gaki.layout.files.rc_search;
        panel_i->config.prompt = cfg->search_prefix;
        if(ac.search_clear) tui_text_line_clear(panel_i->text);
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
            if(tui_rect_encloses_point(sync->panel_gaki.layout.files.rc, input->mouse.pos)) {
                if(input->mouse.scroll > 0) {
                    ac.select_down = 1;
                } else if(input->mouse.scroll < 0) {
                    ac.select_up = 1;
                }
            }
        }
        if(nav && input->mouse.l.down) {
            if(tui_rect_encloses_point(sync->panel_gaki.layout.files.rc, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync->panel_gaki.layout.files.rc, input->mouse.pos);
                nav_directory_select_at(nav, pt.y + nav->offset);
                //nav->index = pt.y + nav->offset;
                any = true;
            }
        }
        if(nav && input->mouse.l.press) {
            if(tui_rect_encloses_point(sync->panel_gaki.layout.parent.rc, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync->panel_gaki.layout.parent.rc, input->mouse.pos);
                if(nav->parent) {
                    Nav_Directory *replace = nav->parent;
                    if(pt.y + replace->offset < array_len(replace->list)) {
                        nav_directory_select_at(replace, pt.y + nav->offset);
                    }
                    sync->panel_gaki.nav_directory = replace;
                    any = true;
                }
            }
            if(tui_rect_encloses_point(sync->panel_gaki.layout.preview.rc, input->mouse.pos)) {
                Tui_Point pt = tui_rect_project_point(sync->panel_gaki.layout.preview.rc, input->mouse.pos);
                if(nav->index < array_len(nav->list)) {
                    Nav_Directory *replace = array_at(nav->list, nav->index);
                    switch(replace->pwd.ref->stats.st_mode & S_IFMT) {
                        case S_IFDIR: {
                            if(pt.y + replace->offset < array_len(replace->list)) {
                                nav_directory_select_at(replace, pt.y + nav->offset);
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

    bool any_shown = nav_directory_visible_count(nav);
    if(any_shown) {

        if(so_len(nav->search.so)) {
            if(ac.search_next) {
                size_t next = nav->index + 1 < array_len(nav->list) ? nav->index + 1 : 0;
                nav_directory_search_next(nav, next, nav->search.so);
                any = true;
            }
            if(ac.search_prev) {
                size_t prev = nav->index ? nav->index - 1 : array_len(nav->list) - 1;
                nav_directory_search_prev(nav, prev, nav->search.so);
                any = true;
            }
        }

        if(ac.select_toggle && nav->index < array_len(nav->list)) {
            Nav_Directory *nav_sub = array_at(nav->list, nav->index);
            nav_sub->pwd.selected = !nav_sub->pwd.selected;
            any = true;
        }

        if(ac.select_up) {
            nav_directory_select_up(sync->panel_gaki.nav_directory, ac.select_up);
            any = true;
        }

        if(ac.select_down) {
            nav_directory_select_down(sync->panel_gaki.nav_directory, ac.select_down);
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

                    switch(nav->pwd.ref->signature_id) {
                        case SO_FILESIG_PNG:
                        case SO_FILESIG_JPEG:
                        case SO_FILESIG_MKV: {

#if 1
                            *flush = true;
                            So prg = so("xdg-open");

                            // /* pause input */
                            // tui_sync_input_idle(sync_i);

                            pid_t pid = fork();
                            if(pid < 0) {
                                // TODO make some kind of notice
                                printff("\rFORK FAILED");
                                exit(1);
                            } else if(!pid) {

                                int fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
                                dup2(fd, 1);
                                dup2(fd, 2);

                                char *cprg = so_dup(prg);
                                char *cpath = so_dup(nav->pwd.ref->path);
                                char *cargs[] = { cprg, cpath, 0 };
                                execvp(cprg, cargs);
                                close(fd);
                                _exit(EXIT_FAILURE);
                            }

                            // int status;
                            // waitpid(pid, &status, 0);

                            // if(status) {
                            //     // TODO make some kind of notice
                            //     printff("\rCHILD FAILED");
                            //     exit(1);
                            // }

                            // /* resume input */
                            // tui_sync_input_wake(sync_i);

                            // /* issue new read */
                            // nav_directory_dispatch_readany(pw, sync_m, sync_t, sync, nav);

                            // /* issue a redraw */
                            // tui_sync_redraw(sync_d);
#endif

                        } break;

                        default: {

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
                            tui_write_cstr(TUI_ESC_CODE_MOUSE_ON);

                            so_free(&nav->pwd.ref->content.text);
                            nav->pwd.have_read = false;
                            nav_directory_dispatch_readany(pw, sync_m, sync_t, sync, nav);

                            /* resume input */
                            tui_sync_input_wake(sync_i);

                            /* unload file */
                            pthread_mutex_lock(&nav->pwd.ref->mtx);
                            so_free(&nav->pwd.ref->content.text);
                            nav->pwd.ref->loaded = false;
                            nav->pwd.ref->loaded_done = false;
                            pthread_mutex_unlock(&nav->pwd.ref->mtx);

                            pthread_mutex_lock(&nav->pwd.mtx);
                            nav->pwd.have_read = false;
                            pthread_mutex_unlock(&nav->pwd.mtx);

                            /* issue new read */
                            nav_directory_dispatch_readany(pw, sync_m, sync_t, sync, nav);

                            /* issue a redraw */
                            tui_sync_redraw(sync_d);
#endif

                        } break;
                    }

                } break;
                default: break;
            }
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
        Nav_Directory *parent = nav ? nav->parent : 0;
        if(parent && parent->pwd.ref && array_len(parent->pwd.ref->content.files)) {
            sync->panel_gaki.nav_directory = nav->parent;
            any = true;
        }
    }

    pthread_mutex_unlock(&sync->mtx);

    *quit = ac.quit;
    return any;
}

void panel_gaki_render_nav_dir(Tui_Buffer *buffer, So *tmp, Nav_Directory *nav, Panel_Gaki *panel, Nav_Directory_Layout layout) {

    /* print file list */
    //Tui_Color sel_fg = { .type = TUI_COLOR_8, .col8 = 0 };
    //Tui_Color sel_bg = { .type = TUI_COLOR_8, .col8 = 7 };
    //Tui_Fx    sel_fx = { .bold = true, .it = true, .ul = true };
    Tui_Color dir_fg = { .type = TUI_COLOR_8, .col8 = 0 };
    Tui_Color dir_bg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Color search_fg = { .type = TUI_COLOR_8, .col8 = 3 };
    Tui_Color search_fg2 = { .type = TUI_COLOR_8, .col8 = 0 };
    Tui_Color search_bg2 = { .type = TUI_COLOR_8, .col8 = 3 };
    Tui_Rect rc = layout.rc;
    rc.dim.y = 1;
    size_t list_len = array_len(nav->list);
    Tui_Buffer_Cache tbc = {0};
    //printff("RENDER: [%.*s] LEN %zu", SO_F(nav->pwd.ref->path), list_len);
    for(size_t i = nav->offset; i < list_len; ++i) {
        if(rc.anc.y >= buffer->dimension.y) break;
        Nav_Directory *nav_sub = array_at(nav->list, i);
        if(!nav_directory_visible_check(nav_sub, nav->filter.so)) continue;
        tbc.fg = (nav->index == i) ? &dir_fg : 0;
        tbc.bg = (nav->index == i) ? &dir_bg : 0;
        //tbc.fx = (nav_sub->pwd.selected) ? &sel_fx : 0;
        //tbc.fx = (nav->index == i) ? &dir_fx : 0;
        tbc.pt = (Tui_Point){0};
        tbc.fill = false;
        tbc.rect = rc;
        so_clear(tmp);
        So name = so_get_nodir(nav_sub->pwd.ref->path);
        if(so_len(nav->search.so)) {
            size_t nfind = so_find_sub(name, nav->search.so, true);
            //printff("\r%zu/%zu:%.*s",nfind, so_len(name),SO_F(name));
            if(nfind < so_len(name)) {

                so_extend(tmp, so_iE(name, nfind));
                tui_buffer_draw_cache(buffer, &tbc, *tmp);
                //printff("\r1 %u,%u",tbc.pt.x,tbc.pt.y);

                tbc.bg = (nav->index == i) ? &search_bg2 : tbc.bg;
                tbc.fg = (nav->index == i) ? &search_fg2 : &search_fg;

                so_clear(tmp);
                so_extend(tmp, nav->search.so);
                tui_buffer_draw_cache(buffer, &tbc, *tmp);
                //printff("\r2 %u,%u",tbc.pt.x,tbc.pt.y);

                tbc.fg = (nav->index == i) ? &dir_fg : 0;
                tbc.bg = (nav->index == i) ? &dir_bg : 0;
                //tbc.fg = (nav->index == i) ? &dir_fg : 0;
                //tbc.bg = (nav->index == i) ? &dir_bg : 0;

                so_clear(tmp);
                so_extend(tmp, so_i0(name, nfind + so_len(nav->search.so)));

                tbc.fill = true;
            } else {
                so_fmt(tmp, "%.*s", SO_F(name));
                tbc.fill = true;
            }
        } else {
            so_fmt(tmp, "%.*s", SO_F(name));
            tbc.fill = true;
        }
        ASSERT_ARG(tbc.fill);
        tui_buffer_draw_cache(buffer, &tbc, *tmp);
        ++rc.anc.y;
    }

    if(nav && nav->search.visual_len) {
        Tui_Color search_fg = { .type = TUI_COLOR_8, .col8 = 0 };
        Tui_Color search_bg = { .type = TUI_COLOR_8, .col8 = 3 };
        so_clear(tmp);
        so_fmt(tmp, " %.*s", SO_F(nav->search.so));
        tui_buffer_draw(buffer, layout.rc_search, &search_fg, &search_bg, 0, *tmp);
    }

    if(nav && nav->filter.visual_len) {
        Tui_Color filter_fg = { .type = TUI_COLOR_8, .col8 = 7 };
        Tui_Color filter_bg = { .type = TUI_COLOR_8, .col8 = 4 };
        so_clear(tmp);
        so_fmt(tmp, " %.*s", SO_F(nav->filter.so));
        tui_buffer_draw(buffer, layout.rc_filter, &filter_fg, &filter_bg, 0, *tmp);
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

    bool any_shown = nav_directory_visible_count(nav);
    panel_gaki_render_nav_dir(buffer, &tmp, nav, panel, panel->layout.files);

    /* draw current dir/file/type */
    Tui_Color bar_bg = { .type = TUI_COLOR_8, .col8 = 1 };
    Tui_Color bar_fg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Fx bar_fx = { .bold = true };
    Nav_Directory *current = nav->index < array_len(nav->list) ? array_at(nav->list, nav->index) : 0;
    size_t tab_len = array_len(panel->tabs);
    if(any_shown && current && nav->pwd.ref) {

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
            case S_IFREG: {
                so_fmt(&tmp, "[FILE:");
                so_filesig_fmt(&tmp, current->pwd.ref->signature_id);
                if(current->pwd.ref->signature_unsure) so_push(&tmp, '?');
                so_fmt(&tmp, "]");
            } break;
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
    if(any_shown && current && current->pwd.ref->loaded) {
        //tui_buffer_draw(buffer, panel->layout.rc_preview, &bar_fg, &bar_bg, &bar_fx, so("HAVE READ"));
        pthread_mutex_lock(&current->pwd.mtx);
        switch(current->pwd.ref->stats.st_mode & S_IFMT) {
            case S_IFREG: {
                if(current->pwd.ref->signature_id == SO_FILESIG_PNG ||
                   current->pwd.ref->signature_id == SO_FILESIG_JPEG) {
                    File_Graphic *gfx = &current->pwd.ref->content.graphic;
                    if(!tui_point_cmp(gfx->cvt_dim, panel->layout.preview.rc.dim)) {
                        tui_buffer_draw_subbuf(buffer, panel->layout.preview.rc, &gfx->cvt_buf);
                    }
                } else {
                    //printff("\r[%.*s]",SO_F(current->pwd.ref->content.text));
                    tui_buffer_draw(buffer, panel->layout.preview.rc, 0, 0, 0, current->pwd.ref->content.text);
                }
            } break;
            case S_IFDIR: {
                panel_gaki_render_nav_dir(buffer, &tmp, current, panel, panel->layout.preview);
            } break;
        }
        pthread_mutex_unlock(&current->pwd.mtx);
    }

    /* draw parent */
    if(nav && nav->parent) {
        panel_gaki_render_nav_dir(buffer, &tmp, nav->parent, panel, panel->layout.parent);
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


