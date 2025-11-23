#include <rltui.h> 
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h> 
#include <rlpw.h>
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

void handle_resize(Tui_Point dimension, Tui_Point pixels, void *void_gaki) {
    Gaki *gaki = void_gaki;

    if(pixels.x && pixels.y) {
        gaki->aspect_ratio_cell_xy = 2 * ((double)pixels.x / (double)dimension.x) / ((double)pixels.y / (double)dimension.y);
    } else {
        gaki->aspect_ratio_cell_xy = 1;
    }

    gaki->sync_panel.panel_gaki.config.rc = (Tui_Rect){ .dim = dimension };
}

bool gaki_update(void *user) {
    Gaki *gaki = user;
    if(gaki->quit) {
        tui_core_quit(gaki->tui);
        return false;
    }
    bool render = true;
    panel_gaki_update(&gaki->sync_panel, &gaki->pw_task, &gaki->sync.main, &gaki->sync_t_file_info, &gaki->panel_input, gaki->aspect_ratio_cell_xy);
    panel_input_update(&gaki->panel_input);
    return render;
}

bool gaki_input(Tui_Input *input, bool *flush, void *user) {
    Gaki *gaki = user;
    bool render = false;
    if(gaki->panel_input.visible) {
        render |= panel_input_input(&gaki->panel_input, &gaki->sync.main, input, flush);
    } else {
        render |= panel_gaki_input(&gaki->sync_panel, &gaki->pw_task, &gaki->sync.main, &gaki->sync_t_file_info, &gaki->sync.input, &gaki->sync.draw, &gaki->config, input, &gaki->panel_input, flush, &gaki->quit);
    }
    return render;
}

void gaki_render(Tui_Buffer *buffer, void *user) {
    Gaki *gaki = user;
    panel_gaki_render(buffer, &gaki->sync_panel);
    panel_input_render(&gaki->panel_input, buffer);
}


int main(int argc, char **argv) {

    Tui_Core_Callbacks callbacks = {
        .input = gaki_input,
        .render = gaki_render,
        .resized = handle_resize,
        .update = gaki_update,
    };

    So out = SO;
    Gaki gaki = {
        .resized = true,
        .config.filter_prefix = so(" "),
        .config.search_prefix = so(" "),
        .tui = tui_core_new(),
    };

    long number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
    pw_init(&gaki.pw_task, number_of_processors ? number_of_processors : 1);
    pw_dispatch(&gaki.pw_task);

    if(argc >= 2) {
        /* TODO iterate over i and make tabs/splits */
        char creal[PATH_MAX];
        realpath(argv[1], creal);
        nav_directory_dispatch_register(&gaki.pw_task, &gaki.sync.main, &gaki.sync_t_file_info, &gaki.sync_panel, so_l(creal));
    } else {
        char ccwd[PATH_MAX];
        getcwd(ccwd, PATH_MAX);
        nav_directory_dispatch_register(&gaki.pw_task, &gaki.sync.main, &gaki.sync_t_file_info, &gaki.sync_panel, so_l(ccwd));
    }

    tui_enter();
    tui_core_init(gaki.tui, &callbacks, &gaki.sync, &gaki);
    while(tui_core_loop(gaki.tui)) {};
    tui_core_free(gaki.tui);
    tui_exit();

    clock_gettime(CLOCK_REALTIME, &gaki.tE);

    t_file_info_free(&gaki.sync_t_file_info.t_file_info);

    gaki_free(&gaki);

    so_free(&out);

#if 0
    double t0 = gaki.t0.tv_sec + gaki.t0.tv_nsec / 1e9;
    double tE = gaki.tE.tv_sec + gaki.tE.tv_nsec / 1e9;
    printff("\rt delta %f", tE-t0);
    printff("\rframes %zu", gaki.frames);
    printff("\r= %f fps / %f ms/frame", (double)gaki.frames/(double)(tE-t0), 1000.0*(double)(tE-t0)/gaki.frames);
#endif

    return 0;
}

