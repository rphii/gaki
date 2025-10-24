#ifndef GAKI_STATE_H

#include <rlso.h>
#include <rltui.h>
#include <rlpw.h>

#include "nav-directory.h"
#include "action.h"

typedef struct Gaki Gaki;

typedef struct Panel_Gaki_Layout {
    Tui_Rect rc_files;
    Tui_Rect rc_split;
    Tui_Rect rc_preview;
    Tui_Rect rc_pwd;
} Panel_Gaki_Layout;

typedef struct Panel_Gaki_Config {
    Tui_Rect rc;
} Panel_Gaki_Config;

typedef struct Panel_Gaki {
    Panel_Gaki_Config config;
    Panel_Gaki_Layout layout;
    Nav_Directory *nav_directory;
    Nav_Directories *tabs;
    size_t tab_sel;
    //So pwd_current;
} Panel_Gaki;

void panel_gaki_select_up(Panel_Gaki *st, size_t n);
void panel_gaki_select_down(Panel_Gaki *st, size_t n);
void panel_gaki_select_at(Panel_Gaki *st, size_t n);
void panel_gaki_update(Pw *pw, Gaki_Sync_Panel *sync, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t);
bool panel_gaki_input(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Tui_Sync_Input *sync_i, Tui_Sync_Draw *sync_d, Tui_Input *input, bool *quit);

void panel_gaki_render(Tui_Buffer *buffer, Gaki_Sync_Panel *gaki);

#define GAKI_STATE_H
#endif

