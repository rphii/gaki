#include <rltui.h> 
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h> 
#include <rlpw.h> 
#include <rlwcwidth.h>
#include <errno.h>

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
    So path;
    union {
        So content;
        struct File_Panel *file_panel;
    };
    struct stat stats;
    size_t offset;
    bool selected;
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

typedef struct File_Panel {
    File_Infos file_infos;
    size_t offset;
    size_t select;
} File_Panel;

void file_panel_free(File_Panel *panel) {

}

LUT_INCLUDE(T_File_Panel, t_file_panel, So, BY_REF, File_Panel, BY_REF);
LUT_IMPLEMENT(T_File_Panel, t_file_panel, So, BY_REF, File_Panel, BY_REF, so_hash_p, so_cmp_p, 0, file_panel_free);

typedef struct Action {
    ssize_t select_up;
    ssize_t select_down;
    ssize_t select_left;
    ssize_t select_right;
} Action;

typedef struct State {
    So tmp;
    So pwd;
    Tui_Rect rc_files;
    Tui_Rect rc_split;
    Tui_Rect rc_preview;
    Tui_Rect rc_pwd;
    File_Panel *file_panel;
    T_File_Panel t_file_infos;
} State;

typedef struct Context2 {
    struct timespec t0;
    struct timespec tE;
    Tui_Input input;
    Tui_Input input_prev;
    Tui_Screen screen;
    Tui_Buffer buffer;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    Pw pw;
    Pw pw_render;
    char *setbuf;
    bool resized;
    bool quit;
    bool drag;
    State st;
    Action ac;

    pthread_cond_t render_cond;
    pthread_mutex_t render_mtx;
    bool render_do;
    bool render_busy;
    size_t frames;

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

#include <dirent.h> 
#include <stdio.h> 

int read_dir(So dirname, File_Panel *panel) {
    DIR *d;
    struct dirent *dir;
    char *cdirname = so_dup(dirname);
    char *cfilename = 0;
    d = opendir(cdirname);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0] == '.') continue;
            File_Info info = {0};
            info.filename = so_clone(so_l(dir->d_name));
            so_path_join(&info.path, dirname, info.filename);
            cfilename = so_dup(info.path);
            stat(cfilename, &info.stats);
            file_infos_push_back(&panel->file_infos, &info);
        }
        closedir(d);
    }
    free(cdirname);
    free(cfilename);
    file_infos_sort(&panel->file_infos);
    return(0);
}

void t_file_infos_set_get(T_File_Panel *t, File_Panel **out, So path) {
    File_Panel *panel = t_file_panel_get(t, &path);
    if(!panel) {
        File_Panel panel_new = {0};
        read_dir(path, &panel_new);
        panel = t_file_panel_once(t, &path, &panel_new)->val;
    }
    *out = panel;
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

void select_up(State *st, size_t n) {
    if(!st->file_panel) return;
    if(!st->file_panel->select) {
        st->file_panel->select = file_infos_length(st->file_panel->file_infos) - 1;
        st->file_panel->offset = st->file_panel->select + 1 > st->rc_files.dim.y ? st->file_panel->select + 1 - st->rc_files.dim.y : 0;
    } else {
        --st->file_panel->select;
        if(st->file_panel->offset) {
            --st->file_panel->offset;
        }
    }
}

void select_down(State *st, size_t n) {
    if(!st->file_panel) return;
    ++st->file_panel->select;
    if(st->file_panel->select >= file_infos_length(st->file_panel->file_infos)) {
        st->file_panel->select = 0;
        st->file_panel->offset = 0;
    }
    if(st->file_panel->select >= st->file_panel->offset + st->rc_files.dim.y) {
        ++st->file_panel->offset;
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

#if 0
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
#endif

void signal_winch(int x) {

    Context2 *ctx = state_global_get();
    pthread_mutex_lock(&ctx->mtx);
    ctx->resized = true;
    pthread_mutex_unlock(&ctx->mtx);
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

    //array_resize(ctx->setbuf, dimension.x * dimension.y);
    //setvbuf(stdout, ctx->setbuf, _IOFBF, dimension.x * dimension.y); // unbuffered stdout

    tui_buffer_resize(&ctx->buffer, dimension);
    //so_fmt(out, TUI_ESC_CODE_CLEAR);
}

Tui_Point tui_rect_project_point(Tui_Rect rc, Tui_Point pt) {
    Tui_Point result = pt;
    result.x -= rc.anc.x;
    result.y -= rc.anc.y;
    return result;
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
                    case 'j': ctx->ac.select_down = 1; break;
                    case 'k': ctx->ac.select_up = 1; break;
                    case 'h': ctx->ac.select_left = 1; break;
                    case 'l': ctx->ac.select_right = 1; break;
                    default: break;
                }
            }
            if(ctx->input.id == INPUT_KEY) {
                if(ctx->input.key == KEY_UP) {
                    ctx->ac.select_up = 1;
                }
                if(ctx->input.key == KEY_DOWN) {
                    ctx->ac.select_down = 1;
                }
                if(ctx->input.key == KEY_LEFT) {
                    ctx->ac.select_left = 1;
                }
                if(ctx->input.key == KEY_RIGHT) {
                    ctx->ac.select_right = 1;
                }
            }
            if(ctx->input.id == INPUT_MOUSE) {
                if(tui_rect_encloses_point(ctx->st.rc_files, ctx->input.mouse.pos)) {
                    if(ctx->input.mouse.scroll > 0) {
                        ctx->ac.select_down = 1;
                    } else if(ctx->input.mouse.scroll < 0) {
                        ctx->ac.select_up = 1;
                    }
                }
                if(ctx->input.mouse.l) {
                    if(tui_rect_encloses_point(ctx->st.rc_files, ctx->input.mouse.pos)) {
                        Tui_Point pt = tui_rect_project_point(ctx->st.rc_files, ctx->input.mouse.pos);
                        if(ctx->st.file_panel) {
                            ctx->st.file_panel->select = pt.y + ctx->st.file_panel->offset;
                        }
                    }
                }
                if(ctx->input.mouse.m) {
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

#if 0
void context_free(Context *ctx) {
    so_free(&ctx->st_ext.dyn->tmp);
    file_infos_free(&ctx->st_ext.dyn->infos);
    array_free(ctx->st_ext.dyn->fx);
    pw_free(&ctx->pw);
}
#endif

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

void render_file_infos(Context2 *ctx, File_Panel *file_panel, Tui_Rect rc) {
    if(!file_panel) return;
    So *tmp = &ctx->st.tmp;
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
        tui_buffer_draw(&ctx->buffer, rc, fg, bg, 0, *tmp);
        ++rc.anc.y;
    }
}

void render(Context2 *ctx) {
    tui_buffer_clear(&ctx->buffer);
    
    State *st = &ctx->st;
    So *tmp = &st->tmp;
    
    /* draw file preview */
#if 1
    so_clear(tmp);
    File_Info *current = 0;
    for(size_t i = 0; st->file_panel && i < file_infos_length(st->file_panel->file_infos); ++i) {
        File_Info *unsel = file_infos_get_at(&ctx->st.file_panel->file_infos, i);
        unsel->selected = false;
    }
    if(st->file_panel && st->file_panel->select < file_infos_length(st->file_panel->file_infos)) {
        current = file_infos_get_at(&ctx->st.file_panel->file_infos, st->file_panel->select);
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
        } else if(S_ISDIR(current->stats.st_mode)) {
            t_file_infos_set_get(&st->t_file_infos, &current->file_panel, current->path);
            render_file_infos(ctx, current->file_panel, st->rc_preview);
            //if(current->file_infos && !so_cmp(current->filename, so("ws"))) {
            if(current->file_panel) {
                //printff("\rgot %u file infos", file_infos_length(*current->file_infos));usleep(1e6);
                //exit(1);
            }
            current->printable = true;
        }
        if(current->printable) {
            tui_buffer_draw(&ctx->buffer, st->rc_preview, 0, 0, 0, current->content);
        }
    }
#endif

#if 1
    /* draw vertical bar */
    so_clear(tmp);
    for(size_t i = 0; i < st->rc_split.dim.y; ++i) {
        so_extend(tmp, so("│\n"));
    }
    tui_buffer_draw(&ctx->buffer, st->rc_split, 0, 0, 0, *tmp);

    /* draw file infos */
    render_file_infos(ctx, st->file_panel, st->rc_files);
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
        tui_buffer_draw(&ctx->buffer, st->rc_pwd, &bar_fg, &bar_bg, &bar_fx, current->path);
        rc_mode.dim.x = tmp->len;
        rc_mode.anc.x = ctx->buffer.dimension.x - rc_mode.dim.x;
        tui_buffer_draw(&ctx->buffer, rc_mode, &bar_fg, &bar_bg, &bar_fx, *tmp);
    } else {
        tui_buffer_draw(&ctx->buffer, st->rc_pwd, &bar_fg, &bar_bg, &bar_fx, st->pwd);
    }

}

Tui_Rect tui_rect(ssize_t anc_x, ssize_t anc_y, ssize_t dim_x, ssize_t dim_y) {
    return (Tui_Rect){
        .anc = (Tui_Point){ .x = anc_x, .y = anc_y },
        .dim = (Tui_Point){ .x = dim_x, .y = dim_y },
    };
}

void update(Context2 *ctx) {
    State *st = &ctx->st;
    st->rc_files.anc.x = 0;
    st->rc_files.anc.y = 1;
    st->rc_files.dim.x = ctx->buffer.dimension.x / 2;
    st->rc_files.dim.y = ctx->buffer.dimension.y - 1;
    st->rc_split.anc.x = st->rc_files.dim.x;
    st->rc_split.anc.y = 1;
    st->rc_split.dim.x = 1;
    st->rc_split.dim.y = ctx->buffer.dimension.y - 1;;
    st->rc_preview.anc.x = st->rc_split.anc.x + 1;
    st->rc_preview.anc.y = 1;
    st->rc_preview.dim.x = ctx->buffer.dimension.x - st->rc_files.anc.x;
    st->rc_preview.dim.y = ctx->buffer.dimension.y - 1;;
    st->rc_pwd.anc.x = 0;
    st->rc_pwd.anc.y = 0; //ctx->buffer.dimension.y - 1;
    st->rc_pwd.dim.x = ctx->buffer.dimension.x;
    st->rc_pwd.dim.y = 1;
    if(ctx->ac.select_left) {
        So left = so_ensure_dir(so_rsplit_ch(st->pwd, PLATFORM_CH_SUBDIR, 0));
        if(left.len) {
            st->pwd = left;
            st->file_panel = 0;
        } else {
            so_copy(&st->pwd, so("/"));
            st->file_panel = 0;
        }
    }
    if(ctx->ac.select_right && st->file_panel) {
        if(st->file_panel->select < file_infos_length(st->file_panel->file_infos)) {
            File_Info *sel = file_infos_get_at(&st->file_panel->file_infos, st->file_panel->select);
            if(S_ISDIR(sel->stats.st_mode)) {
                so_path_join(&st->pwd, st->pwd, sel->filename);
                st->file_panel = 0;
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
    if(!st->file_panel) {
        t_file_infos_set_get(&st->t_file_infos, &st->file_panel, st->pwd);
    }
    if(ctx->ac.select_up) {
        select_up(st, ctx->ac.select_up);
    }
    if(ctx->ac.select_down) {
        select_down(st, ctx->ac.select_down);
    }
    memset(&ctx->ac, 0, sizeof(ctx->ac));
}

void *pw_queue_render(Pw *pw, bool *quit, void *void_ctx) {
    Context2 *ctx = void_ctx;
    So draw = SO;
    while(!*quit) {
        bool render_do = false;
        bool render_busy = false;

        pthread_mutex_lock(&ctx->render_mtx);
        render_busy = ctx->render_busy;
        ctx->render_do = false;
        ctx->render_busy = false;
        if(!ctx->render_busy) {
            pthread_cond_wait(&ctx->render_cond, &ctx->render_mtx);
        }
        render_do = ctx->render_do;
        pthread_mutex_unlock(&ctx->render_mtx);

        if(render_busy) {
            pthread_mutex_lock(&ctx->mtx);
            pthread_cond_signal(&ctx->cond);
            pthread_mutex_unlock(&ctx->mtx);
            continue;
        }

        if(!render_do) continue;

        so_clear(&draw);
        tui_screen_fmt(&draw, &ctx->screen);

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
        //fflush(stdout);
        ++ctx->frames;

        //so_file_write(so("out.txt"), draw);
        //tui_write_so(draw);
    }
    return 0;
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

    pw_init(&ctx.pw_render, 1);
    pw_queue(&ctx.pw_render, pw_queue_render, &ctx);
    pw_dispatch(&ctx.pw_render);

    fast_srand(time(0));

    //for(size_t i = 0; i < 1000; ++i) {
    clock_gettime(CLOCK_REALTIME, &ctx.t0);
    for(;;) {
        if(ctx.quit) break;
        handle_resize(&ctx);
        update(&ctx);
        render(&ctx);

#if 1
        pthread_mutex_lock(&ctx.render_mtx);
        if(!ctx.render_do) {
            if(tui_point_cmp(ctx.screen.dimension, ctx.buffer.dimension)) {
                tui_screen_resize(&ctx.screen, ctx.buffer.dimension);
            }
            memcpy(ctx.screen.now.cells, ctx.buffer.cells, sizeof(Tui_Cell) * ctx.buffer.dimension.x * ctx.buffer.dimension.y);
            ctx.render_do = true;
        } else {
            //printff("RENDER BUSY");exit(1);
            ctx.render_busy = true;
        }
        pthread_mutex_unlock(&ctx.render_mtx);
        pthread_cond_signal(&ctx.render_cond);
#endif

#if 1
        pthread_mutex_lock(&ctx.mtx);
        if(!ctx.resized) {
            //ctx.ac.select_down = 1;
            pthread_cond_wait(&ctx.cond, &ctx.mtx);
        }
        pthread_mutex_unlock(&ctx.mtx);
#endif
    }

    clock_gettime(CLOCK_REALTIME, &ctx.tE);

    //context_free(&ctx);

    so_free(&out);
    tui_exit();

#if 0
    double t0 = ctx.t0.tv_sec + ctx.t0.tv_nsec / 1e9;
    double tE = ctx.tE.tv_sec + ctx.tE.tv_nsec / 1e9;
    printff("\rt delta %f", tE-t0);
    printff("\rframes %zu", ctx.frames);
    printff("\r= %f fps / %f ms/frame", (double)ctx.frames/(double)(tE-t0), 1000.0*(double)(tE-t0)/ctx.frames);
#endif

    return 0;
}

