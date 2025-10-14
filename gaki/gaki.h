#ifndef GAKI_H

#include <rltui.h>
#include <rlpw.h>

#include "panel-gaki.h"
#include "action.h"

#if 0
typedef enum {
    GAKI_IDLE,
    GAKI_SCHEDULE,
    GAKI_BUSY,
    GAKI_DONE,
} Gaki_List;
#endif

typedef struct Gaki_Sync_Main {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    unsigned int update_do;
    unsigned int update_done;
    unsigned int render_do;
    unsigned int render_done;
} Gaki_Sync_Main;

typedef struct Gaki_Sync_Draw {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    unsigned int draw_do;
    unsigned int draw_skip;
    unsigned int draw_done;
} Gaki_Sync_Draw;

typedef struct Gaki {
    struct timespec t0;
    struct timespec tE;
    size_t frames;

    Tui_Input input;
    Tui_Input input_prev;
    Tui_Screen screen;
    Tui_Buffer buffer;
    // bool main_update;
    // bool main_updated;

    Gaki_Sync_Main sync_main;
    Gaki_Sync_Draw sync_draw;
    //Gaki_Sync sync_render;

    // pthread_cond_t draw_cond;
    // pthread_mutex_t draw_mtx;
    // bool draw_dirty;
    // Gaki_List draw_stage;
    // // bool draw_do;
    // // bool draw_busy;

    Pw pw_main;
    Pw pw_draw;
    Pw pw_task;

    Panel_Gaki panel_gaki;
    Action ac;

    _Atomic bool resized;
    bool quit;

    // bool render;
    // bool rendered;



} Gaki;

Gaki *gaki_global_get(void);
void gaki_global_set(Gaki *gaki);
void gaki_free(Gaki *gaki);
void gaki_update(Gaki *gaki);

#define GAKI_H
#endif

