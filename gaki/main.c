#include <rltui.h> 
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h> 
#include <rlpw.h> 
#include <rlwcwidth.h>

void tui_fmt_clear_line(So *out, size_t n) {
    if(n) {
        so_fmt(out, "\x1b[s");
        so_fmt(out, "%*s", n, "");
        so_fmt(out, "\x1b[u");
    }
}

typedef struct Fx {
    unsigned char *col;
} Fx;

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

void fmt_fx_on(So *out, Fx *fx) {
    so_fmt(out, FS_BEG);
    for(size_t i = 0; i < array_len(fx); ++i) {
        Fx f = array_at(fx, i);
        if(f.col) {
            so_fmt(out, "%s", f.col);
        }
    }
    so_push(out, 'm');
}

void fmt_fx_off(So *out) {
    so_fmt(out, FS_BEG "0m");
}


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> 
 
typedef struct Render {
    bool all;
    bool spinner;
    bool header;
    bool filenames;
    bool select;
    bool split;
    bool preview;
} Render;

typedef struct File_Info {
    bool printable;
    So filename;
    So content;
    struct stat stats;
} File_Info;

int file_info_cmp(File_Info *a, File_Info *b) {
    return so_cmp_s(a->filename, b->filename);
}

void file_info_free(File_Info *a) {
    so_free(&a->filename);
    so_free(&a->content);
}

VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, BASE);
VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, SORT);
VEC_IMPLEMENT_BASE(File_Infos, file_infos, File_Info, BY_REF, file_info_free);
VEC_IMPLEMENT_SORT(File_Infos, file_infos, File_Info, BY_REF, file_info_cmp);

typedef struct State_Dynamic {
    File_Infos infos;
    So tmp;
    Fx *fx;
} State_Dynamic;

typedef struct State {
    State_Dynamic *dyn;
    So_Align_Cache *alc;
    size_t offset;
    size_t select;
    Tui_Point dimension;
    struct {
        So_Align filenames;
        So_Align preview;
        So_Align header;
        size_t split;
    } al;
    Tui_Rect rect_files;
    Tui_Rect rect_preview;
    Render render;
    size_t wave_left;
    size_t wave_right;
    size_t wave_width;
    bool wave_reverse;
    struct timeval t;
    double t_delta_wave;
    bool quit;
} State;

typedef struct Context2 {
    Tui_Input input;
    Tui_Input input_prev;
    Tui_Screen screen;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    Pw pw;
    bool resized;
    bool quit;
} Context2;

typedef struct Context {
    struct timespec refresh;
    Tui_Input input;
    Tui_Input input_prev;
    pthread_cond_t cond;
    State st_ext;
    bool do_ext;
    pthread_mutex_t mtx;
    So_Align_Cache alc;
    Pw pw;
} Context;

bool render_any(Render *render) {
    bool result = memcmp(render, &(Render){0}, sizeof(Render));
    if(render->all) {
        memset(render, true, sizeof(Render));
    }
    return result;
}

#include <dirent.h> 
#include <stdio.h> 

int read_dir(So dirname, File_Infos *infos) {
    DIR *d;
    struct dirent *dir;
    char *cdirname = so_dup(dirname);
    d = opendir(cdirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            File_Info info = {0};
            info.filename = so_clone(so_l(dir->d_name));
            stat(dir->d_name, &info.stats);
            file_infos_push_back(infos, &info);
        }
        closedir(d);
    }
    free(cdirname);
    file_infos_sort(infos);
    return(0);
}

size_t file_info_rel(File_Info *info) {
    size_t result;
    off_t sz = info->stats.st_size;
    for(result = 1; sz >= result * 1000; result *= 1000) {}
    return result;
}

double file_info_relsize(File_Info *info) {
    size_t factor = file_info_rel(info);
    //double result = info->stats.st_dev / (double)factor;
    double result = (double)info->stats.st_size / (double)factor;
    //double result = info->stats.st_size;
    return result;
}
char *file_info_relcstr(File_Info *info) {
    size_t factor = file_info_rel(info);
    switch(factor) {
        case 1ULL: return "B";
        case 1000ULL: return "KiB";
        case 1000000ULL: return "MiB";
        case 1000000000ULL: return "GiB";
        case 1000000000000ULL: return "TiB";
        case 1000000000000000ULL: return "PiB";
        case 1000000000000000000ULL: return "EiB";
        default: break;
    }
    return "??B";
}

#define array_back(x)   array_at((x), array_len(x) - 1)

void render_split(So *out, State *st, size_t y) {
    so_fmt(out, TUI_ESC_CODE_GOTO(st->al.split, y));
    //so_extend(out, so(F("│", BG_BK_B FG_YL_B)));
    so_extend(out, so("│"));
}

void render_file_info(So *out, State *st, File_Info *info) {
    so_clear(&st->dyn->tmp);

    Fx fx = (Fx){ IT FG_CY };
    array_push(st->dyn->fx, fx);
    fmt_fx_on(&st->dyn->tmp, st->dyn->fx);
    so_fmt(&st->dyn->tmp, "%3.0f %-3s", file_info_relsize(info), file_info_relcstr(info));
    array_pop(st->dyn->fx);

    fmt_fx_on(&st->dyn->tmp, st->dyn->fx);
    so_fmt(&st->dyn->tmp, " %.*s ", SO_F(info->filename));

    fmt_fx_on(&st->dyn->tmp, st->dyn->fx);

    so_al_cache_clear(st->alc);
    so_extend_al(out, st->al.filenames, 0, st->dyn->tmp);
}

void select_up(State *st) {
    if(!st->select) {
        st->select = file_infos_length(st->dyn->infos) - 1;
        st->offset = st->select + 1 > st->rect_files.dimension.y ? st->select + 1 - st->rect_files.dimension.y : 0;
        if(st->offset) st->render.filenames = true;
    } else {
        --st->select;
        if(st->offset) {
            --st->offset;
            st->render.filenames = true;
        }
    }
}

void select_down(State *st) {
    ++st->select;
    if(st->select >= file_infos_length(st->dyn->infos)) {
        st->select = 0;
        if(st->offset) st->render.filenames = true;
        st->offset = 0;
    }
    if(st->select >= st->offset + st->rect_files.dimension.y) {
        ++st->offset;
        st->render.filenames = true;
    }
}

void select_at(State *st, size_t n) {
}

#include <signal.h>


static Context2 *state_global;

Context2 *state_global_get(void) {
    return state_global;
}

void state_global_set(Context2 *st) {
    state_global = st;
}

void state_apply_split(State *st) {
#if 1
    st->rect_preview = (Tui_Rect){
        .anchor.x = 0, .anchor.y = 1,
        .dimension.y = st->dimension.y - 1, .dimension.x = st->al.split,
    };
    st->rect_files = (Tui_Rect){
        .anchor.x = st->al.split + 1, .anchor.y = 1,
        .dimension.y = st->dimension.y - 1, .dimension.x = st->dimension.x - st->al.split - 1,
    };
#else
    st->rect_files = (Tui_Rect){
        .anchor.x = 0, .anchor.y = 1,
        .dimension.y = st->dimension.y - 1, .dimension.x = st->al.split,
    };
    st->rect_preview = (Tui_Rect){
        .anchor.x = st->al.split + 1, .anchor.y = 1,
        .dimension.y = st->dimension.y - 1, .dimension.x = st->al.split - 1,
    };
#endif
    so_al_config(&st->al.filenames, 0, 0, st->rect_files.dimension.x, 1, st->alc);
    so_al_config(&st->al.preview, 0, 0, st->rect_preview.dimension.x, 1, st->alc);
    so_al_config(&st->al.header, 0, 0, st->dimension.x, 1, st->alc);
}

void signal_winch(int x) {

    Context2 *ctx = state_global_get();
    ctx->resized = true;
    pthread_cond_signal(&ctx->cond);
}

void handle_resize(Context2 *ctx) {
    if(!ctx->resized) return;
    ctx->resized = false;
    
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    Tui_Point dimension = {
        .x = w.ws_col,
        .y = w.ws_row,
    };

    tui_screen_resize(&ctx->screen, dimension);
}

void *pw_queue_process_input(Pw *pw, bool *quit, void *void_ctx) {
    Context2 *ctx = void_ctx;
    bool resize_split = false;
    for(;;) {
        ctx->input_prev = ctx->input;
        //printff("getting input..\r");
        if(ctx->quit) break;
        if(tui_input_process(&ctx->input)) {
            if(ctx->input.id == INPUT_TEXT && ctx->input.text.len == 1) {
                switch(ctx->input.text.str[0]) {
                    case 'q': ctx->quit = true; break;
                    default:
                              break;
                }
            }
            if(ctx->input.id == INPUT_KEY) {
                if(ctx->input.key == KEY_UP) {
                }
                if(ctx->input.key == KEY_DOWN) {
                }
            }
            if(ctx->input.id == INPUT_MOUSE) {
                if(ctx->input.mouse.scroll > 0) {
                } else if(ctx->input.mouse.scroll < 0) {
                }
                if(ctx->input.mouse.l || ctx->input.mouse.m) {
                }
                if(ctx->input.mouse.r) {
                    if(!ctx->input_prev.mouse.r) {
                    }
                    if(resize_split) {
                    }
                } else {
                    resize_split = false;
                }
            }
            pthread_cond_signal(&ctx->cond);
        }
    }
    return 0;
}

void context_free(Context *ctx) {
    so_free(&ctx->st_ext.dyn->tmp);
    file_infos_free(&ctx->st_ext.dyn->infos);
    array_free(ctx->st_ext.dyn->fx);
    pw_free(&ctx->pw);
}

static unsigned int g_seed;

// Used to seed the generator.           
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

void render(Context2 *ctx) {
    Tui_Rect rc = {
        .anc = (Tui_Point){ .x = 2, .y = 1 },
        .dim = (Tui_Point){ .x = ctx->screen.dimension.x - 8, .y = ctx->screen.dimension.y - 4 },
        //.dim = (Tui_Point){ .x = 1, .y = 1 }
    };
    Tui_Point pt;
    Tui_Rect cnv = { .dim = ctx->screen.dimension };
    --cnv.dim.x;
    --cnv.dim.y;
    for(pt.y = rc.anc.y; pt.y < rc.anc.y + rc.dim.y; ++pt.y) {
        for(pt.x = rc.anc.x; pt.x < rc.anc.x + rc.dim.x; ++pt.x) {
            if(!tui_rect_contains_point(cnv, pt)) continue;
            //printff("\rRENDER %u,%u", pt.x,pt.y);
            Tui_Cell *cell = tui_buffer_at(&ctx->screen.now, pt);
            cell->ucp.val = fast_rand() % ('z'-'A') + 'A';
        }
    }
}

int main(void) {

    tui_enter();

    So out = SO;
    Context2 ctx = { .resized = true };

    state_global_set(&ctx);

    signal(SIGWINCH, signal_winch);

    pw_init(&ctx.pw, 1);
    pw_queue(&ctx.pw, pw_queue_process_input, &ctx);
    pw_dispatch(&ctx.pw);

#if 1
    for(;;) {
        if(ctx.quit) break;
        handle_resize(&ctx);
        render(&ctx);
        so_clear(&out);
        tui_screen_fmt(&out, &ctx.screen);
        //printf("\r");
        //so_printdbg(out);
        tui_write_so(out);
        pthread_cond_wait(&ctx.cond, &ctx.mtx);
    }
#else
    for(;;) {
        State st = ctx.st_ext;
        if(st.quit) break;
        /* render - prepare */
        so_clear(&out);
        gettimeofday(&st.t, NULL);
        so_fmt(&out, TUI_ESC_CODE_CURSOR_HIDE);
        /* render - draw */
        render_any(&st.render);
        if(st.render.all) {
            so_fmt(&out, TUI_ESC_CODE_CLEAR);
        }
        if(st.render.spinner) {
            so_al_cache_clear(st.alc);
            time_t rawtime;
            struct tm *timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            size_t i_loading = (size_t)((st.t.tv_sec + (double)st.t.tv_usec * 1e-6) * 10);

            Fx fx = (Fx){ BOLD BG_BK_B FG_YL_B };
            array_push(st.dyn->fx, fx);
            so_fmt(&out, TUI_ESC_CODE_GOTO(0, 0));
            fmt_fx_on(&out, st.dyn->fx);
            so_clear(&st.dyn->tmp);
            so_fmt(&st.dyn->tmp, "%c gaki %c %4u-%02u-%02u %02u:%02u:%02u ", \
                    loading_set[(i_loading + (sizeof(loading_set)-1)/2) % (sizeof(loading_set)-1)], \
                    loading_set[i_loading % (sizeof(loading_set)-1)], \
                    1900+timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday,
                    timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec
                    );
            so_extend_al(&out, st.al.header, 0, st.dyn->tmp);

            st.wave_width = st.dimension.x > ctx.alc.progress ? st.dimension.x - ctx.alc.progress : 0;
            double delta = timeval_d(st.t) - timeval_d(stdiff.t);
            double t_delta_wave_step = 0.1;
            st.t_delta_wave += delta;
            for(double t = 0; t + t_delta_wave_step < st.t_delta_wave; t += t_delta_wave_step) {
                st.t_delta_wave -= t_delta_wave_step;
                if(!st.wave_reverse) {
                    if(st.wave_right < st.wave_width) {
                        ++st.wave_right;
                    } else if(st.wave_left < st.wave_width) {
                        ++st.wave_left;
                        if(st.wave_left >= st.wave_width) {
                            st.wave_reverse = true;
                        }
                    }
                } else {
                    if(st.wave_left > 0) {
                        --st.wave_left;
                    } else if(st.wave_right > 0) {
                        --st.wave_right;
                        if(!st.wave_right) {
                            st.wave_reverse = false;
                        }
                    }
                }
            }

            for(size_t i = 0; i < st.wave_width; ++i) {
                if((st.wave_left <= st.wave_right && i >= st.wave_left && i <= st.wave_right)) {
                    so_extend_al(&out, st.al.header, 0, so("."));
                } else {
                    so_extend_al(&out, st.al.header, 0, so(" "));
                }
            }
            fmt_fx_off(&out);
            array_pop(st.dyn->fx);
        }
        if(st.render.filenames) {
            if(!file_infos_length(st.dyn->infos)) {
                file_infos_free(&st.dyn->infos);
                read_dir(so("."), &st.dyn->infos);
            }
            for(size_t i = st.offset; i < file_infos_length(st.dyn->infos); ++i) {
                if(i - st.offset < st.rect_files.dimension.y) {
                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_files.anchor.x, st.rect_files.anchor.y + i - st.offset));
                    tui_fmt_clear_line(&out, st.rect_files.dimension.x);
                    File_Info *info = file_infos_get_at(&st.dyn->infos, i);
                    so_clear(&st.dyn->tmp);
                    so_extend_al(&out, st.al.filenames, 0, st.dyn->tmp);
                    render_file_info(&out, &st, info);
                }
            }
        }
        if(st.render.select) {
            st.render.preview = true;
            if(!st.render.filenames && stdiff.select < file_infos_length(st.dyn->infos)) {
                if(stdiff.select - stdiff.offset < st.rect_files.dimension.y) {
                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_files.anchor.x, st.rect_files.anchor.y + stdiff.select - stdiff.offset));
                    tui_fmt_clear_line(&out, st.rect_files.dimension.x);
                    File_Info *info = file_infos_get_at(&st.dyn->infos, stdiff.select);
                    render_file_info(&out, &st, info);
                    render_split(&out, &st, st.rect_files.anchor.y + stdiff.select);
                }
            }
            if(st.select < file_infos_length(st.dyn->infos)) {
                if(st.select - st.offset < st.rect_files.dimension.y) {
                    Fx fx = { BG_WT_B FG_BK };
                    array_push(st.dyn->fx, fx);

                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_files.anchor.x, st.rect_files.anchor.y + st.select - st.offset));
                    File_Info *info = file_infos_get_at(&st.dyn->infos, st.select);
                    render_file_info(&out, &st, info);

                    for(size_t i = 0; i < st.dimension.x; ++i) {
                        so_extend_al(&out, st.al.filenames, 0, so(" "));
                    }

                    array_pop(st.dyn->fx);
                    fmt_fx_off(&out);
                    render_split(&out, &st, st.rect_files.anchor.y + st.select);
                }
            }
        }
        if(st.render.split) {
            for(size_t i = 0; i < st.rect_files.dimension.y; ++i) {
                render_split(&out, &st, st.rect_files.anchor.y + i);
            }
        }
        if(st.render.preview) {
            File_Info *info = file_infos_get_at(&st.dyn->infos, st.select);
            if(so_is_empty(info->content)) {
                so_file_read(info->filename, &info->content);
                info->printable = true;
                for(size_t i = 0; i < so_len(info->content); ++i) {
                    unsigned char c = so_at(info->content, i);
                    if(!(c >= ' ' || isspace(c))) {
                        info->printable = false;
                        break;
                    }
                }
            }
            size_t line_nb = 0;
            if(info->printable) {
                for(So line = SO; so_splice(info->content, &line, '\n'); ) {
                    if(line_nb + st.rect_preview.anchor.y >= st.dimension.y) break;
                    //printff("LINE:%.*s",SO_F(line));
                    if(so_is_zero(line)) continue;
                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_preview.anchor.x, line_nb + st.rect_preview.anchor.y));
                    tui_fmt_clear_line(&out, st.rect_preview.dimension.x);
                    so_al_cache_clear(st.alc);
                    so_extend_al(&out, st.al.preview, 0, line);
                    ++line_nb;
                }
            } else {
                so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_preview.anchor.x, st.rect_preview.anchor.y));
                tui_fmt_clear_line(&out, st.rect_preview.dimension.x);
                so_al_cache_clear(st.alc);
                so_extend_al(&out, st.al.preview, 0, so(F("file contains non-printable characters", IT)));
                ++line_nb;
            }
            while(line_nb < st.rect_preview.dimension.y) {
                so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_preview.anchor.x, line_nb + st.rect_preview.anchor.y));
                tui_fmt_clear_line(&out, st.rect_preview.dimension.x);
                ++line_nb;
            }
        }
        if(!ctx.do_ext) {
            /* render - out */
            tui_write_so(out);
            /* render - reset */
            stdiff = st;
            st.render = (Render){0};
            st.render.spinner = true;
            ctx.st_ext = st;
            pthread_cond_wait(&ctx.cond, &ctx.mtx);
        }
        ctx.do_ext = false;

        //struct timeval now;
        //gettimeofday(&now,NULL);
        //struct timespec until = timespec_add_timeval(ctx.refresh, now);
        //pthread_cond_timedwait(&ctx.cond, &ctx.mtx, &until);
    }

    context_free(&ctx);
#endif

    so_free(&out);
    tui_exit();
    return 0;
}

