#ifndef GAKI_STATE_H

#include <rlso.h>
#include <rltui.h>
#include <rlpw.h>

#include "nav-directory.h"
#include "gaki-config.h"
#include "action.h"

typedef struct Gaki Gaki;
typedef struct Panel_Input Panel_Input;

typedef struct Nav_Directory_Layout {
    Tui_Rect rc;
    Tui_Rect rc_filter;
    Tui_Rect rc_search;
} Nav_Directory_Layout;

typedef struct Panel_Gaki_Layout {
    Tui_Rect rc_pwd;
    Nav_Directory_Layout files;
    Nav_Directory_Layout parent;
    Nav_Directory_Layout preview;
    Tui_Rect rc_split_parent;
    Tui_Rect rc_split_preview;
} Panel_Gaki_Layout;

typedef struct Panel_Gaki_Config {
    Tui_Rect rc;
    unsigned int ratio_files;
    unsigned int ratio_parent;
    unsigned int ratio_preview;
} Panel_Gaki_Config;

typedef struct Panel_Gaki {
    Panel_Gaki_Config config;
    Panel_Gaki_Layout layout;
    Nav_Directory *nav_directory;
    Nav_Directories *tabs;
    size_t tab_sel;
    //So pwd_current;
} Panel_Gaki;

void panel_gaki_update(Gaki_Sync_Panel *sync, Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Panel_Input *panel_i);
bool panel_gaki_input(Gaki_Sync_Panel *sync, Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Tui_Sync_Input *sync_i, Tui_Sync_Draw *sync_d, Gaki_Config *cfg, Tui_Input *input, Panel_Input *panel_i, bool *flush, bool *quit);
void panel_gaki_render(Tui_Buffer *buffer, Gaki_Sync_Panel *gaki);

#define GAKI_STATE_H
#endif

