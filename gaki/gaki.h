#ifndef GAKI_H

#include <rltui.h>
#include <rlpw.h>

#include "panel-gaki.h"
#include "action.h"
#include "gaki-sync.h"

#if 0
typedef enum {
    GAKI_IDLE,
    GAKI_SCHEDULE,
    GAKI_BUSY,
    GAKI_DONE,
} Gaki_List;
#endif

typedef struct Gaki {
    struct timespec t0;
    struct timespec tE;
    size_t frames;

    Tui_Input input_curr;
    Tui_Input input_prev;
    Tui_Screen screen;
    Tui_Buffer buffer;

    Gaki_Sync_Main sync_main;
    Gaki_Sync_Draw sync_draw;
    Gaki_Sync_Input sync_input;

    Pw pw_main;
    Pw pw_draw;
    Pw pw_task;

    Panel_Gaki panel_gaki;
    Action ac;

    bool quit;

    _Atomic bool resized; // can't really use pthreads in a signal, so let's just do this one atomic... not perfect. should maybe re-visit one day

} Gaki;

Gaki *gaki_global_get(void);
void gaki_global_set(Gaki *gaki);
void gaki_free(Gaki *gaki);
void gaki_update(Gaki *gaki);

#define GAKI_H
#endif

