#if 0
#ifndef FILE_PANEL_H

#include <rltui.h>

#include "file-info.h"

typedef struct Panel_File {
    File_Infos file_infos;
    size_t offset;
    size_t select;
} Panel_File;

typedef struct Gaki Gaki;

void panel_file_free(Panel_File *panel);
void panel_file_render(Tui_Buffer *buffer, Tui_Rect rc, Panel_File *panel_file);

#define FILE_PANEL_H
#endif
#endif

