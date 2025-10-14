#ifndef FILE_PANEL_H

#include <rltui.h>

#include "file-info.h"

typedef struct Panel_File {
    File_Infos file_infos;
    size_t offset;
    size_t select;
    So tmp;
    bool read;
} Panel_File;

typedef struct Gaki Gaki;

LUT_INCLUDE(T_Panel_File, t_panel_file, So, BY_REF, Panel_File, BY_REF);

void panel_file_free(Panel_File *panel);
void panel_file_render(Tui_Buffer *buffer, Tui_Rect rc, Panel_File *panel_file);
void t_panel_file_ensure_exist(T_Panel_File *t, Panel_File **out, So path);

#define FILE_PANEL_H
#endif

