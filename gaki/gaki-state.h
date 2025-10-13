#ifndef GAKI_STATE_H

#include <rlso.h>
#include <rltui.h>

#include "file-panel.h"
#include "action.h"

typedef struct Gaki_State {
    So tmp;
    So pwd;
    Tui_Rect rc_files;
    Tui_Rect rc_split;
    Tui_Rect rc_preview;
    Tui_Rect rc_pwd;
    Tui_Point pt_dimension;
    File_Panel *file_panel;
    T_File_Panel t_file_infos;
} Gaki_State;

void gaki_state_select_up(Gaki_State *st, size_t n);
void gaki_state_select_down(Gaki_State *st, size_t n);
void gaki_state_select_at(Gaki_State *st, size_t n);
void gaki_state_update(Gaki_State *st, Action *ac);

#define GAKI_STATE_H
#endif

