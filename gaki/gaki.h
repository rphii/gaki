#ifndef GAKI_H

#include <rltui.h>
#include <rlpw.h>

#include "gaki-config.h"
#include "panel-gaki.h"
#include "panel-input.h"
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
    Gaki_Config config;

    struct timespec t0;
    struct timespec tE;
    size_t frames;

    Panel_Input panel_input;

    struct Tui_Core *tui;
    Tui_Sync sync;
    Gaki_Sync_Panel sync_panel;

    Gaki_Sync_T_File_Info sync_t_file_info;

    double aspect_ratio_cell_xy;

    Pw pw_main;
    Pw pw_draw;
    Pw pw_task;

    bool quit;

    _Atomic bool resized; // can't really use pthreads in a signal, so let's just do this one atomic... not perfect. should maybe re-visit one day

} Gaki;

void gaki_free(Gaki *gaki);

#define GAKI_H
#endif

