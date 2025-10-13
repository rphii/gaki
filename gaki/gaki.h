#ifndef GAKI_H

#include <rltui.h>
#include <rlpw.h>

#include "panel-gaki.h"
#include "action.h"

typedef struct Gaki {
    struct timespec t0;
    struct timespec tE;
    size_t frames;

    Tui_Input input;
    Tui_Input input_prev;
    Tui_Screen screen;
    Tui_Buffer buffer;

    pthread_cond_t main_cond;
    pthread_mutex_t main_mtx;
    bool main_update;

    pthread_cond_t draw_cond;
    pthread_mutex_t draw_mtx;
    bool draw_do;
    bool draw_busy;

    Pw pw_main;
    Pw pw_draw;
    Pw pw_task;

    Panel_Gaki panel_gaki;
    Action ac;

    bool resized;
    bool quit;

} Gaki;

Gaki *gaki_global_get(void);
void gaki_global_set(Gaki *gaki);
void gaki_free(Gaki *gaki);
void gaki_update(Gaki *gaki);

#define GAKI_H
#endif

