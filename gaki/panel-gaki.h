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
    //So pwd_current;
} Panel_Gaki;

void panel_gaki_select_up(Panel_Gaki *st, size_t n);
void panel_gaki_select_down(Panel_Gaki *st, size_t n);
void panel_gaki_select_at(Panel_Gaki *st, size_t n);
void panel_gaki_update(Gaki_Sync_Panel *sync, Action *ac);

void panel_gaki_render(Tui_Buffer *buffer, Gaki_Sync_Panel *gaki);

#define GAKI_STATE_H
#endif

