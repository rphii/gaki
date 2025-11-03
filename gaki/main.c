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
    gaki->resized = true;
    pthread_cond_signal(&gaki->sync_main.cond);
}

void handle_resize(Gaki *gaki) {
    if(!gaki->resized) return;
    
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    Tui_Point dimension = {
        .x = w.ws_col,
        .y = w.ws_row,
    };

    Tui_Point dimension_prev = gaki->buffer.dimension;
    if(!tui_point_cmp(dimension_prev, dimension)) {
        gaki->resized = false;
        return;
    }

    gaki->sync_panel.panel_gaki.config.rc = (Tui_Rect){ .dim = dimension };
    tui_buffer_resize(&gaki->buffer, dimension);
    tui_sync_main_render(&gaki->sync_main);
}

void *pw_queue_process_input(Pw *pw, bool *quit, void *void_ctx) {
    Gaki *gaki = void_ctx;
    for(;;) {

#if 0
        Tui_Input sim = { .id = INPUT_TEXT };
        int rnd = fast_rand() % 5;
        //if(rnd == 0) sim.text.val = 'h';
        if(rnd == 1) sim.text.val = 'j';
        //if(rnd == 2) sim.text.val = 'k';
        //if(rnd == 3) sim.text.val = 'l';
        pthread_mutex_lock(&gaki->sync_input.mtx);
        array_push(gaki->sync_input.inputs, sim);
        pthread_mutex_unlock(&gaki->sync_input.mtx);
        tui_sync_main_update(&gaki->sync_main);
#endif

        if(!tui_input_process(&gaki->sync_main, &gaki->sync_input, &gaki->input_gen)) break;
    }
    return 0;
}

void *pw_queue_render(Pw *pw, bool *quit, void *void_ctx) {
    Gaki *gaki = void_ctx;
    So draw = SO;

    while(!*quit) {

        pthread_mutex_lock(&gaki->sync_draw.mtx);
        ++gaki->sync_draw.draw_done;
        if(gaki->sync_draw.draw_done >= gaki->sync_draw.draw_do) {
            gaki->sync_draw.draw_do = 0;
            gaki->sync_draw.draw_done = 0;
        }
        while(!gaki->sync_draw.draw_do && !gaki->sync_draw.draw_skip) {
            pthread_cond_wait(&gaki->sync_draw.cond, &gaki->sync_draw.mtx);
        }
        bool draw_busy = gaki->sync_draw.draw_skip;
        bool draw_do = gaki->sync_draw.draw_do;
        bool draw_redraw = gaki->sync_draw.draw_redraw;
        gaki->sync_draw.draw_skip = 0;
        //if(draw_busy && gaki->frames > 4) exit(1);
        if(draw_do && !draw_busy && draw_redraw) {
            gaki->sync_draw.draw_redraw = 0;
        }
        pthread_mutex_unlock(&gaki->sync_draw.mtx);

        if(draw_busy) {
            pthread_mutex_lock(&gaki->sync_main.mtx);
            ++gaki->sync_main.render_do;
            pthread_cond_signal(&gaki->sync_main.cond);
            pthread_mutex_unlock(&gaki->sync_main.mtx);
            continue;
        }

        if(!draw_do) continue;

        so_clear(&draw);

        if(draw_redraw) {
            memset(gaki->screen.old.cells, 0, sizeof(Tui_Cell) * gaki->buffer.dimension.x * gaki->buffer.dimension.y);
            so_extend(&draw, so(TUI_ESC_CODE_CURSOR_HIDE));
        }

        tui_screen_fmt(&draw, &gaki->screen);

        ssize_t len = draw.len;
        ssize_t written = 0;
        char *begin = so_it0(draw);
        while(written < len) {
            //break;
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

int main(int argc, char **argv) {

    tui_enter();

    So out = SO;
    Gaki gaki = { .resized = true, .sync_main.update_do = true };

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

    if(argc >= 2) {
        /* TODO iterate over i and make tabs/splits */
        nav_directory_dispatch_register(&gaki.pw_task, &gaki.sync_main, &gaki.sync_t_file_info, &gaki.sync_panel, so_l(argv[1]));
    } else {
        So pwd = SO;
        so_env_get(&pwd, so("PWD"));
        nav_directory_dispatch_register(&gaki.pw_task, &gaki.sync_main, &gaki.sync_t_file_info, &gaki.sync_panel, pwd);
        so_free(&pwd);
    }

    fast_srand(time(0));

    //for(size_t i = 0; i < 1000; ++i) {
    clock_gettime(CLOCK_REALTIME, &gaki.t0);
    while(!gaki.quit) {
        
        pthread_mutex_lock(&gaki.sync_main.mtx);
        bool update_do = gaki.sync_main.update_done < gaki.sync_main.update_do;
        //printff("\rupdate do:%u",update_do);
        pthread_mutex_unlock(&gaki.sync_main.mtx);

        if(update_do) {
            handle_resize(&gaki);

            bool render = false;
            tui_input_get_stack(&gaki.sync_input, &gaki.inputs);
            bool flush = false;
            while(!gaki.quit && array_len(gaki.inputs)) {
                Tui_Input input = array_pop(gaki.inputs);
                if(gaki.panel_input.visible) {
                    render |= panel_input_input(&gaki.panel_input, &gaki.sync_main, &input, &flush);
                } else {
                    render |= panel_gaki_input(&gaki.sync_panel, &gaki.pw_task, &gaki.sync_main, &gaki.sync_t_file_info, &gaki.sync_input, &gaki.sync_draw, &input, &gaki.panel_input, &flush, &gaki.quit);
                }
                if(flush) continue;
            }
            panel_gaki_update(&gaki.sync_panel, &gaki.pw_task, &gaki.sync_main, &gaki.sync_t_file_info, &gaki.panel_input);
            panel_input_update(&gaki.panel_input);

            pthread_mutex_lock(&gaki.sync_main.mtx);
            ++gaki.sync_main.update_done;
            if(render || !gaki.frames) ++gaki.sync_main.render_do;
            pthread_mutex_unlock(&gaki.sync_main.mtx);
        }

        if(gaki.quit) break;

        bool draw_busy = true;
        pthread_mutex_lock(&gaki.sync_draw.mtx);
        draw_busy = gaki.sync_draw.draw_done < gaki.sync_draw.draw_do;
        //printff("\rdraw busy:%u",draw_busy);
        if(draw_busy) {
            ++gaki.sync_draw.draw_skip;
        }
        pthread_mutex_unlock(&gaki.sync_draw.mtx);

        pthread_mutex_lock(&gaki.sync_main.mtx);
        bool render_do = gaki.sync_main.render_done < gaki.sync_main.render_do;
        //printff("\rrender do:%u",render_do);
        if(draw_busy) {
            gaki.sync_main.render_do = gaki.sync_main.render_done;
        }
        pthread_mutex_unlock(&gaki.sync_main.mtx);

        if(render_do && !draw_busy) {
            tui_buffer_clear(&gaki.buffer);
            panel_gaki_render(&gaki.buffer, &gaki.sync_panel);
            panel_input_render(&gaki.panel_input, &gaki.buffer);

#if 0
            So tmp = SO;
            so_fmt(&tmp, "[%zu]", gaki.frames);
            tui_buffer_draw(&gaki.buffer, (Tui_Rect){ .dim = gaki.screen.dimension}, 0, 0, 0, tmp);
            so_free(&tmp);
#endif

            pthread_mutex_lock(&gaki.sync_main.mtx);
            ++gaki.sync_main.render_done;
            pthread_mutex_unlock(&gaki.sync_main.mtx);

            pthread_mutex_lock(&gaki.sync_draw.mtx);
            ++gaki.sync_draw.draw_do;
            if(tui_point_cmp(gaki.screen.dimension, gaki.buffer.dimension)) {
                tui_screen_resize(&gaki.screen, gaki.buffer.dimension);
            }
            memcpy(gaki.screen.now.cells, gaki.buffer.cells, sizeof(Tui_Cell) * gaki.buffer.dimension.x * gaki.buffer.dimension.y);
            pthread_cond_signal(&gaki.sync_draw.cond);
            pthread_mutex_unlock(&gaki.sync_draw.mtx);

        }
#if 1
        pthread_mutex_lock(&gaki.sync_main.mtx);
        if(gaki.sync_main.render_done >= gaki.sync_main.render_do) {
            gaki.sync_main.render_do = 0;
            gaki.sync_main.render_done = 0;
        }
        if(gaki.sync_main.update_done >= gaki.sync_main.update_do) {
            gaki.sync_main.update_do = 0;
            gaki.sync_main.update_done = 0;
        }
        while(!gaki.sync_main.update_do && !gaki.sync_main.render_do) {
            if(gaki.resized) {
                gaki.sync_main.update_do = true;
                break;
            } else {
                pthread_cond_wait(&gaki.sync_main.cond, &gaki.sync_main.mtx);
            }
        }
        pthread_mutex_unlock(&gaki.sync_main.mtx);
#endif
    }

    clock_gettime(CLOCK_REALTIME, &gaki.tE);

    tui_sync_input_quit(&gaki.sync_input);

    t_file_info_free(&gaki.sync_t_file_info.t_file_info);
    array_free(gaki.inputs);
    array_free(gaki.sync_input.inputs);
    tui_screen_free(&gaki.screen);
    tui_buffer_free(&gaki.buffer);

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

