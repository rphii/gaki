#ifndef GAKI_STATE_H

#include <rlso.h>
#include <rltui.h>

#include "panel-file.h"
#include "action.h"

typedef struct Panel_Gaki {
    So tmp;
    So pwd;
    Tui_Rect rc_files;
    Tui_Rect rc_split;
    Tui_Rect rc_preview;
    Tui_Rect rc_pwd;
    Tui_Point pt_dimension;
    Panel_File *panel_file;
    T_Panel_File t_file_infos;
} Panel_Gaki;

void panel_gaki_select_up(Panel_Gaki *st, size_t n);
void panel_gaki_select_down(Panel_Gaki *st, size_t n);
void panel_gaki_select_at(Panel_Gaki *st, size_t n);
void panel_gaki_update(Panel_Gaki *st, Action *ac);

#define GAKI_STATE_H
#endif

