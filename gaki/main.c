#include <rltui.h> 
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h> 
#include <rlpw.h> 
#include <rlwcwidth.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h> 

#include "gaki.h"

double timeval_d(struct timeval v) {
    return (double)v.tv_sec + (double)v.tv_usec * 1e-6;
}

struct timespec timespec_add_timespec(struct timespec a, struct timespec b) {
    struct timespec result;
    result.tv_sec = a.tv_sec + b.tv_sec;
    result.tv_nsec = a.tv_nsec + b.tv_nsec;
    if(result.tv_nsec < a.tv_nsec || result.tv_nsec < b.tv_nsec) ++result.tv_sec;
    return result;
}

struct timespec timespec_add_timeval(struct timespec a, struct timeval b) {
    struct timespec bt = {
        .tv_nsec = b.tv_usec * 1e3,
        .tv_sec = b.tv_sec,
    };
    struct timespec result = timespec_add_timespec(a, bt);
    return result;
}


#include <signal.h>

void signal_winch(int x) {

    Gaki *gaki = gaki_global_get();
    pthread_mutex_lock(&gaki->mtx);
    gaki->resized = true;
    pthread_mutex_unlock(&gaki->mtx);
    pthread_cond_signal(&gaki->cond);
}

void handle_resize(Gaki *gaki) {
    if(!gaki->resized) return;
    gaki->resized = false;
    
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    Tui_Point dimension = {
        .x = w.ws_col,
        .y = w.ws_row,
    };

    gaki->st.pt_dimension = dimension;
    tui_buffer_resize(&gaki->buffer, dimension);
}

Tui_Point tui_rect_project_point(Tui_Rect rc, Tui_Point pt) {
    Tui_Point result = pt;
    result.x -= rc.anc.x;
    result.y -= rc.anc.y;
    return result;
}

void *pw_queue_process_input(Pw *pw, bool *quit, void *void_ctx) {
    Gaki *gaki = void_ctx;
    bool resize_split = false;
    for(;;) {
        gaki->input_prev = gaki->input;
        //printff("getting input..\r");
        if(gaki->quit) break;
        if(tui_input_process(&gaki->input)) {
            if(gaki->input.id == INPUT_TEXT && gaki->input.text.len == 1) {
                switch(gaki->input.text.str[0]) {
                    case 'q': gaki->quit = true; break;
                    case 'j': gaki->ac.select_down = 1; break;
                    case 'k': gaki->ac.select_up = 1; break;
                    case 'h': gaki->ac.select_left = 1; break;
                    case 'l': gaki->ac.select_right = 1; break;
                    default: break;
                }
            }
            if(gaki->input.id == INPUT_KEY) {
                if(gaki->input.key == KEY_UP) {
                    gaki->ac.select_up = 1;
                }
                if(gaki->input.key == KEY_DOWN) {
                    gaki->ac.select_down = 1;
                }
                if(gaki->input.key == KEY_LEFT) {
                    gaki->ac.select_left = 1;
                }
                if(gaki->input.key == KEY_RIGHT) {
                    gaki->ac.select_right = 1;
                }
            }

            if(gaki->input.id == INPUT_MOUSE) {
                if(tui_rect_encloses_point(gaki->st.rc_files, gaki->input.mouse.pos)) {
                    if(gaki->input.mouse.scroll > 0) {
                        gaki->ac.select_down = 1;
                    } else if(gaki->input.mouse.scroll < 0) {
                        gaki->ac.select_up = 1;
                    }
                }
                if(gaki->input.mouse.l) {
                    if(tui_rect_encloses_point(gaki->st.rc_files, gaki->input.mouse.pos)) {
                        Tui_Point pt = tui_rect_project_point(gaki->st.rc_files, gaki->input.mouse.pos);
                        if(gaki->st.file_panel) {
                            gaki->st.file_panel->select = pt.y + gaki->st.file_panel->offset;
                        }
                    }
                }
                if(gaki->input.mouse.m) {
                }
                if(gaki->input.mouse.r) {
                    if(!gaki->input_prev.mouse.r) {
                    }
                    if(resize_split) {
                    }
                } else {
                    resize_split = false;
                }
            }
            pthread_cond_signal(&gaki->cond);
        }
    }
    return 0;
}

static unsigned int g_seed;

// Used to seed the generator.           
void fast_srand(int seed);
inline void fast_srand(int seed) {
    g_seed = seed;
}

// Compute a pseudorandom integer.
// Output value in range [0, 32767]
inline int fast_rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}
int fast_rand(void);

void render_file_infos(Gaki *gaki, File_Panel *file_panel, Tui_Rect rc) {
    if(!file_panel) return;
    So *tmp = &gaki->st.tmp;
    /* draw file infos */
    Tui_Color sel_bg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Color sel_fg = { .type = TUI_COLOR_8, .col8 = 0 };
    ssize_t dim_y = rc.dim.y;
    rc.dim.y = 1;
    for(size_t i = file_panel->offset; i < file_infos_length(file_panel->file_infos); ++i) {
        if(i >= file_panel->offset + dim_y) break;
        File_Info *info = file_infos_get_at(&file_panel->file_infos, i);
        Tui_Color *fg = info->selected ? &sel_fg : 0;
        Tui_Color *bg = info->selected ? &sel_bg : 0;
        so_clear(tmp);
        so_extend(tmp, info->filename);
        so_push(tmp, '\n');
        tui_buffer_draw(&gaki->buffer, rc, fg, bg, 0, *tmp);
        ++rc.anc.y;
    }
}

void render(Gaki *gaki) {
    tui_buffer_clear(&gaki->buffer);
    
    Gaki_State *st = &gaki->st;
    So *tmp = &st->tmp;
    
    /* draw file preview */
#if 1
    so_clear(tmp);
    File_Info *current = 0;
    for(size_t i = 0; st->file_panel && i < file_infos_length(st->file_panel->file_infos); ++i) {
        File_Info *unsel = file_infos_get_at(&gaki->st.file_panel->file_infos, i);
        unsel->selected = false;
    }
    if(st->file_panel && st->file_panel->select < file_infos_length(st->file_panel->file_infos)) {
        current = file_infos_get_at(&gaki->st.file_panel->file_infos, st->file_panel->select);
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
                tui_buffer_draw(&gaki->buffer, st->rc_preview, 0, 0, 0, current->content);
            }
        } else if(S_ISDIR(current->stats.st_mode)) {
            t_file_panel_ensure_exist(&st->t_file_infos, &current->file_panel, current->path);
            current->printable = true;
            render_file_infos(gaki, current->file_panel, st->rc_preview);
        }
    }
#endif

#if 1
    /* draw vertical bar */
    so_clear(tmp);
    for(size_t i = 0; i < st->rc_split.dim.y; ++i) {
        so_extend(tmp, so("│\n"));
    }
    tui_buffer_draw(&gaki->buffer, st->rc_split, 0, 0, 0, *tmp);

    /* draw file infos */
    render_file_infos(gaki, st->file_panel, st->rc_files);
#endif

    /* draw current dir/file/type */
    Tui_Color bar_bg = { .type = TUI_COLOR_8, .col8 = 1 };
    Tui_Color bar_fg = { .type = TUI_COLOR_8, .col8 = 7 };
    Tui_Fx bar_fx = { .bold = true };
    if(current) {
        so_clear(tmp);
        Tui_Rect rc_mode = st->rc_pwd;
        if(S_ISDIR(current->stats.st_mode)) {
            so_fmt(tmp, "[DIR]");
            bar_bg.col8 = 4;
        } else if(S_ISREG(current->stats.st_mode)) {
            so_fmt(tmp, "[FILE]");
            bar_bg.col8 = 5;
        } else {
            so_fmt(tmp, "[?]");
        }
        tui_buffer_draw(&gaki->buffer, st->rc_pwd, &bar_fg, &bar_bg, &bar_fx, current->path);
        rc_mode.dim.x = tmp->len;
        rc_mode.anc.x = gaki->buffer.dimension.x - rc_mode.dim.x;
        tui_buffer_draw(&gaki->buffer, rc_mode, &bar_fg, &bar_bg, &bar_fx, *tmp);
    } else {
        tui_buffer_draw(&gaki->buffer, st->rc_pwd, &bar_fg, &bar_bg, &bar_fx, st->pwd);
    }

}

Tui_Rect tui_rect(ssize_t anc_x, ssize_t anc_y, ssize_t dim_x, ssize_t dim_y) {
    return (Tui_Rect){
        .anc = (Tui_Point){ .x = anc_x, .y = anc_y },
        .dim = (Tui_Point){ .x = dim_x, .y = dim_y },
    };
}

void *pw_queue_render(Pw *pw, bool *quit, void *void_ctx) {
    Gaki *gaki = void_ctx;
    So draw = SO;
    while(!*quit) {
        bool render_do = false;
        bool render_busy = false;

        pthread_mutex_lock(&gaki->render_mtx);
        render_busy = gaki->render_busy;
        gaki->render_do = false;
        gaki->render_busy = false;
        if(!gaki->render_busy) {
            pthread_cond_wait(&gaki->render_cond, &gaki->render_mtx);
        }
        render_do = gaki->render_do;
        pthread_mutex_unlock(&gaki->render_mtx);

        if(render_busy) {
            pthread_mutex_lock(&gaki->mtx);
            pthread_cond_signal(&gaki->cond);
            pthread_mutex_unlock(&gaki->mtx);
            continue;
        }

        if(!render_do) continue;

        so_clear(&draw);
        tui_screen_fmt(&draw, &gaki->screen);

        ssize_t len = draw.len;
        ssize_t written = 0;
        char *begin = so_it0(draw);
        while(written < len) {
            errno = 0;
            ssize_t written_chunk = write(STDOUT_FILENO, begin, len - written);
            if(written_chunk > 0) {
                written += written_chunk;
                begin += written_chunk;
            } else {
                if(errno) {
                    printff("\rerrno on write: %u", errno);exit(1);
                } else {
                    continue;
                }
            }
        }
        ++gaki->frames;
    }
    return 0;
}

int main(void) {

    tui_enter();

    So out = SO;
    Gaki gaki = { .resized = true };

    gaki_global_set(&gaki);

    signal(SIGWINCH, signal_winch);

    pw_init(&gaki.pw, 1);
    pw_queue(&gaki.pw, pw_queue_process_input, &gaki);
    pw_dispatch(&gaki.pw);

    pw_init(&gaki.pw_render, 1);
    pw_queue(&gaki.pw_render, pw_queue_render, &gaki);
    pw_dispatch(&gaki.pw_render);

    fast_srand(time(0));

    //for(size_t i = 0; i < 1000; ++i) {
    clock_gettime(CLOCK_REALTIME, &gaki.t0);
    for(;;) {
        if(gaki.quit) break;
        handle_resize(&gaki);
        gaki_state_update(&gaki.st, &gaki.ac);
        render(&gaki);

#if 1
        pthread_mutex_lock(&gaki.render_mtx);
        if(!gaki.render_do) {
            if(tui_point_cmp(gaki.screen.dimension, gaki.buffer.dimension)) {
                tui_screen_resize(&gaki.screen, gaki.buffer.dimension);
            }
            memcpy(gaki.screen.now.cells, gaki.buffer.cells, sizeof(Tui_Cell) * gaki.buffer.dimension.x * gaki.buffer.dimension.y);
            gaki.render_do = true;
        } else {
            //printff("RENDER BUSY");exit(1);
            gaki.render_busy = true;
        }
        pthread_mutex_unlock(&gaki.render_mtx);
        pthread_cond_signal(&gaki.render_cond);
#endif

#if 1
        pthread_mutex_lock(&gaki.mtx);
        if(!gaki.resized) {
            //gaki.ac.select_down = 1;
            pthread_cond_wait(&gaki.cond, &gaki.mtx);
        }
        pthread_mutex_unlock(&gaki.mtx);
#endif
    }

    clock_gettime(CLOCK_REALTIME, &gaki.tE);

    gaki_free(&gaki);

    so_free(&out);
    tui_exit();

#if 0
    double t0 = gaki.t0.tv_sec + gaki.t0.tv_nsec / 1e9;
    double tE = gaki.tE.tv_sec + gaki.tE.tv_nsec / 1e9;
    printff("\rt delta %f", tE-t0);
    printff("\rframes %zu", gaki.frames);
    printff("\r= %f fps / %f ms/frame", (double)gaki.frames/(double)(tE-t0), 1000.0*(double)(tE-t0)/gaki.frames);
#endif

    return 0;
}

