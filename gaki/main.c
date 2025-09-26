#include <rltui.h> 
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>

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
}

VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, BASE);
VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, SORT);
VEC_IMPLEMENT_BASE(File_Infos, file_infos, File_Info, BY_REF, file_info_free);
VEC_IMPLEMENT_SORT(File_Infos, file_infos, File_Info, BY_REF, file_info_cmp);

typedef struct State {
    File_Infos infos;
    size_t offset;
    size_t select;
    Tui_Point dimension;
    So_Align_Cache alc;
    struct {
        So_Align filenames;
        So_Align preview;
        So_Align header;
        size_t split;
    } al;
    Tui_Rect rect_files;
    Tui_Rect rect_preview;
    So tmp;
    Fx *fx;
    Render render;
    size_t wave_left;
    size_t wave_right;
    size_t wave_width;
    bool wave_reverse;
    struct timeval t;
    double t_delta_wave;
    bool quit;
} State;

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
    so_extend(out, so(F("│", BG_BK_B FG_YL_B)));
}

void render_file_info(So *out, State *st, File_Info *info) {
    so_al_cache_clear(&st->alc);
    so_clear(&st->tmp);

    Fx fx = (Fx){ IT FG_CY };
    array_push(st->fx, fx);
    fmt_fx_on(&st->tmp, st->fx);
    so_fmt(&st->tmp, "%3.0f %-3s", file_info_relsize(info), file_info_relcstr(info));
    array_pop(st->fx);

    fmt_fx_on(&st->tmp, st->fx);
    so_fmt(&st->tmp, " %.*s ", SO_F(info->filename));

    fmt_fx_on(&st->tmp, st->fx);

    so_extend_al(out, st->al.filenames, 0, st->tmp);
}

void select_up(State *st) {
    if(!st->select) {
        st->select = file_infos_length(st->infos) - 1;
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
    if(st->select >= file_infos_length(st->infos)) {
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


static State *state_global;

State *state_global_get(void) {
    return state_global;
}

void state_global_set(State *st) {
    state_global = st;
}

void signal_winch(int x) {
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    State *st = state_global_get();
    State stdiff = *st;
    st->dimension.x = w.ws_col;
    st->dimension.y = w.ws_row;
    st->al.split = st->dimension.x / 2;
    st->rect_files = (Tui_Rect){
        .anchor.x = 0, .anchor.y = 1,
        .dimension.y = st->dimension.y - 1, .dimension.x = st->al.split,
    };
    st->rect_preview = (Tui_Rect){
        .anchor.x = st->al.split + 1, .anchor.y = 1,
        .dimension.y = st->dimension.y - 1, .dimension.x = st->al.split - 1,
    };
    so_al_config(&st->al.filenames, 0, 0, st->rect_files.dimension.x, 1, &st->alc);
    so_al_config(&st->al.preview, 0, 0, st->rect_preview.dimension.x, 1, &st->alc);
    so_al_config(&st->al.header, 0, 0, st->dimension.x, 1, &st->alc);
    st->render.all = true;

    /* patch selection */
    if(st->select >= st->offset + st->rect_files.dimension.y) {
        st->offset = st->select + 1 > st->rect_files.dimension.y ? st->select + 1 - st->rect_files.dimension.y : 0;
    }

    /* patch wave */
    if(stdiff.wave_width) {
        size_t wave_diff = st->dimension.x - stdiff.dimension.x;
        st->wave_left = 0;
        st->wave_right = 0;
    }
}

int main(void) {

    tui_enter();
    Tui_Input input = {0};

    signal(SIGWINCH, signal_winch);

    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    char loading_set[] = "-<([|])>->)]|[(<";
    bool exit = false;
    bool loading = true;

    So out = SO;
    So banner = so_l(F("fo", BG_BK_B));
    State st = {0};
    State stdiff = {0};
    state_global_set(&st);
    signal_winch(0);
    
    gettimeofday(&stdiff.t, NULL);
    st.t = stdiff.t;

    while(!st.quit) {
        if(!st.render.all && tui_input_process(&input)) {
            //printff("INPUT ID %u",input.id);
            if(input.id == INPUT_TEXT && input.text.len == 1) {
                switch(input.text.str[0]) {
                    case 'q': st.quit = true; break;
                    case 'r': 
                              st.render.all = true;
                              break;
                    case 'j':
                              st.render.select = true;
                              select_down(&st);
                              break;
                    case 'k':
                              st.render.select = true;
                              select_up(&st);
                              break;
                    case 'g': 
                              st.render.select = true;
                              st.select = 0;
                              if(st.offset) st.render.filenames = true;
                              st.offset = 0;
                              break;
                    case 'G': 
                              st.render.select = true;
                              st.select = file_infos_length(st.infos) - 1;
                              st.offset = st.select + 1 > st.rect_files.dimension.y ? st.select + 1 - st.rect_files.dimension.y : 0;
                              if(st.offset) st.render.filenames = true;
                              break;
                    default:
                              break;
                }
            }
            if(input.id == INPUT_KEY) {
                if(input.key == KEY_UP) {
                    st.render.select = true;
                    select_up(&st);
                }
                if(input.key == KEY_DOWN) {
                    st.render.select = true;
                    select_down(&st);
                }
            }
            if(input.id == INPUT_MOUSE) {
                if(input.mouse.scroll > 0) {
                    st.render.select = true;
                    select_down(&st);
                } else if(input.mouse.scroll < 0) {
                    st.render.select = true;
                    select_up(&st);
                }
                if(input.mouse.l || input.mouse.m || input.mouse.r) {
                    size_t y = input.mouse.pos.y - st.rect_files.anchor.y + st.offset;
                    if(y < file_infos_length(st.infos)) st.select = y;
                    st.render.select = true;
                }
            }
        }
        if(!render_any(&st.render)) continue;
        /* render - prepare */
        so_clear(&out);
        gettimeofday(&st.t, NULL);
        so_fmt(&out, TUI_ESC_CODE_CURSOR_HIDE);
        /* render - draw */
        if(st.render.all) {
            so_fmt(&out, TUI_ESC_CODE_CLEAR);
        }
        if(st.render.spinner) {
            so_al_cache_clear(&st.alc);
            time_t rawtime;
            struct tm *timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            size_t i_loading = (size_t)((st.t.tv_sec + (double)st.t.tv_usec * 1e-6) * 10);

            Fx fx = (Fx){ BOLD BG_BK_B FG_YL_B };
            array_push(st.fx, fx);
            so_fmt(&out, TUI_ESC_CODE_GOTO(0, 0));
            fmt_fx_on(&out, st.fx);
            so_clear(&st.tmp);
            so_fmt(&st.tmp, "%c gaki %c %4u-%02u-%02u %02u:%02u:%02u ", \
                    loading_set[(i_loading + (sizeof(loading_set)-1)/2) % (sizeof(loading_set)-1)], \
                    loading_set[i_loading % (sizeof(loading_set)-1)], \
                    1900+timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday,
                    timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec
                    );
            so_extend_al(&out, st.al.header, 0, st.tmp);

            st.wave_width = st.dimension.x > st.alc.progress ? st.dimension.x - st.alc.progress : 0;
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
            array_pop(st.fx);
        }
        if(st.render.filenames) {
            file_infos_free(&st.infos);
            read_dir(so("."), &st.infos);
            for(size_t i = st.offset; i < file_infos_length(st.infos); ++i) {
                if(i - st.offset < st.rect_files.dimension.y) {
                    Fx fx = {0};
                    array_push(st.fx, fx);

                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_files.anchor.x, st.rect_files.anchor.y + i - st.offset));
                    tui_fmt_clear_line(&out, st.rect_files.dimension.x);
                    File_Info *info = file_infos_get_at(&st.infos, i);
                    render_file_info(&out, &st, info);

                    array_pop(st.fx);
                    fmt_fx_off(&out);
                }
            }
        }
        if(st.render.select) {
            st.render.preview = true;
            if(!st.render.filenames && stdiff.select < file_infos_length(st.infos)) {
                if(stdiff.select - stdiff.offset < st.rect_files.dimension.y) {
                    Fx fx = {0};
                    array_push(st.fx, fx);

                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_files.anchor.x, st.rect_files.anchor.y + stdiff.select - stdiff.offset));
                    tui_fmt_clear_line(&out, st.rect_files.dimension.x);
                    File_Info *info = file_infos_get_at(&st.infos, stdiff.select);
                    render_file_info(&out, &st, info);

                    array_pop(st.fx);
                    fmt_fx_off(&out);
                    render_split(&out, &st, st.rect_files.anchor.y + stdiff.select);
                }
            }
            if(st.select < file_infos_length(st.infos)) {
                if(st.select - st.offset < st.rect_files.dimension.y) {
                    Fx fx = { BG_WT_B FG_BK };
                    array_push(st.fx, fx);

                    so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_files.anchor.x, st.rect_files.anchor.y + st.select - st.offset));
                    File_Info *info = file_infos_get_at(&st.infos, st.select);
                    render_file_info(&out, &st, info);

                    for(size_t i = 0; i < st.dimension.x; ++i) {
                        so_extend_al(&out, st.al.filenames, 0, so(" "));
                    }

                    array_pop(st.fx);
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
            File_Info *info = file_infos_get_at(&st.infos, st.select);
            if(so_is_empty(info->content)) {
                so_file_read(info->filename, &info->content);
                info->printable = true;
                for(size_t i = 0; i < so_len(info->content); ++i) {
                    unsigned char c = so_at(info->content, i);
                    if(!(c >= ' ' || isspace(c))) {
                        so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_preview.anchor.x, 1 + st.rect_preview.anchor.y));
                        so_fmt(&out, TUI_ESC_CODE_CLEAR_TO_END);
                        so_clear(&st.tmp);
                        so_fmt(&st.tmp, F(" %#0x", IT), c);
                        so_extend_al(&out, st.al.preview, 0, st.tmp);
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
                    so_fmt(&out, TUI_ESC_CODE_CLEAR_TO_END);
                    so_al_cache_clear(&st.alc);
                    so_extend_al(&out, st.al.preview, 0, line);
                    ++line_nb;
                }
            } else {
                so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_preview.anchor.x, st.rect_preview.anchor.y));
                so_fmt(&out, TUI_ESC_CODE_CLEAR_TO_END);
                so_extend_al(&out, st.al.preview, 0, so(F("file contains non-printable characters", IT)));
                line_nb = 2;
            }
            while(line_nb < st.rect_preview.dimension.y) {
                so_fmt(&out, TUI_ESC_CODE_GOTO(st.rect_preview.anchor.x, line_nb + st.rect_preview.anchor.y));
                so_fmt(&out, TUI_ESC_CODE_CLEAR_TO_END);
                line_nb++;
            }
        }
        /* render - out */
        tui_write_so(out);
        /* render - reset */
        stdiff = st;
        st.render = (Render){0};
        st.render.spinner = true;
    }

    tui_exit();
    return 0;
}

