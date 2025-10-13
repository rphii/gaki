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

    pthread_mutex_lock(&gaki->main_mtx);
    gaki->resized = true;
    gaki->main_update = true;
    pthread_cond_signal(&gaki->main_cond);
    pthread_mutex_unlock(&gaki->main_mtx);
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

    gaki->panel_gaki.config.rc = (Tui_Rect){ .dim = dimension };

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
                if(tui_rect_encloses_point(gaki->panel_gaki.layout.rc_files, gaki->input.mouse.pos)) {
                    if(gaki->input.mouse.scroll > 0) {
                        gaki->ac.select_down = 1;
                    } else if(gaki->input.mouse.scroll < 0) {
                        gaki->ac.select_up = 1;
                    }
                }
                if(gaki->input.mouse.l) {
                    if(tui_rect_encloses_point(gaki->panel_gaki.layout.rc_files, gaki->input.mouse.pos)) {
                        Tui_Point pt = tui_rect_project_point(gaki->panel_gaki.layout.rc_files, gaki->input.mouse.pos);
                        if(gaki->panel_gaki.panel_file) {
                            gaki->panel_gaki.panel_file->select = pt.y + gaki->panel_gaki.panel_file->offset;
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
            pthread_mutex_lock(&gaki->main_mtx);
            gaki->main_update = true;
            pthread_cond_signal(&gaki->main_cond);
            pthread_mutex_unlock(&gaki->main_mtx);
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
        bool draw_do = false;
        bool draw_busy = false;

        pthread_mutex_lock(&gaki->draw_mtx);
        draw_busy = gaki->draw_busy;
        gaki->draw_do = false;
        gaki->draw_busy = false;
        if(!gaki->draw_busy) {
            pthread_cond_wait(&gaki->draw_cond, &gaki->draw_mtx);
        }
        draw_do = gaki->draw_do;
        pthread_mutex_unlock(&gaki->draw_mtx);

        if(draw_busy) {
            pthread_mutex_lock(&gaki->main_mtx);
            gaki->main_update = true;
            pthread_cond_signal(&gaki->main_cond);
            pthread_mutex_unlock(&gaki->main_mtx);
            continue;
        }

        if(!draw_do) continue;

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
    Gaki gaki = { .resized = true, .main_update = true };

    gaki_global_set(&gaki);

    signal(SIGWINCH, signal_winch);

    pw_init(&gaki.pw_main, 1);
    pw_queue(&gaki.pw_main, pw_queue_process_input, &gaki);
    pw_dispatch(&gaki.pw_main);

    pw_init(&gaki.pw_draw, 1);
    pw_queue(&gaki.pw_draw, pw_queue_render, &gaki);
    pw_dispatch(&gaki.pw_draw);

    pw_init(&gaki.pw_task, 4);
    pw_dispatch(&gaki.pw_task);

    fast_srand(time(0));

    //for(size_t i = 0; i < 1000; ++i) {
    clock_gettime(CLOCK_REALTIME, &gaki.t0);
    for(;;) {
        if(gaki.quit) break;
        handle_resize(&gaki);

        panel_gaki_update(&gaki.panel_gaki, &gaki.ac);

        tui_buffer_clear(&gaki.buffer);
        panel_gaki_render(&gaki.buffer, &gaki.panel_gaki);

#if 1
        pthread_mutex_lock(&gaki.draw_mtx);
        if(!gaki.draw_do) {
            if(tui_point_cmp(gaki.screen.dimension, gaki.buffer.dimension)) {
                tui_screen_resize(&gaki.screen, gaki.buffer.dimension);
            }
            memcpy(gaki.screen.now.cells, gaki.buffer.cells, sizeof(Tui_Cell) * gaki.buffer.dimension.x * gaki.buffer.dimension.y);
            gaki.draw_do = true;
        } else {
            //printff("RENDER BUSY");exit(1);
            gaki.draw_busy = true;
        }
        pthread_mutex_unlock(&gaki.draw_mtx);
        pthread_cond_signal(&gaki.draw_cond);
#endif

#if 1
        pthread_mutex_lock(&gaki.main_mtx);
        if(!gaki.main_update) {
            //gaki.ac.select_down = 1;
            pthread_cond_wait(&gaki.main_cond, &gaki.main_mtx);
        }
        gaki.main_update = false;
        pthread_mutex_unlock(&gaki.main_mtx);
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

