#include "gaki-state.h"

void gaki_state_select_up(Gaki_State *st, size_t n) {
    if(!st->file_panel) return;
    if(!st->file_panel->select) {
        st->file_panel->select = file_infos_length(st->file_panel->file_infos) - 1;
        st->file_panel->offset = st->file_panel->select + 1 > st->rc_files.dim.y ? st->file_panel->select + 1 - st->rc_files.dim.y : 0;
    } else {
        --st->file_panel->select;
        if(st->file_panel->offset) {
            --st->file_panel->offset;
        }
    }
}

void gaki_state_select_down(Gaki_State *st, size_t n) {
    if(!st->file_panel) return;
    ++st->file_panel->select;
    if(st->file_panel->select >= file_infos_length(st->file_panel->file_infos)) {
        st->file_panel->select = 0;
        st->file_panel->offset = 0;
    }
    if(st->file_panel->select >= st->file_panel->offset + st->rc_files.dim.y) {
        ++st->file_panel->offset;
    }
}

void gaki_state_select_at(Gaki_State *st, size_t n) {
}

