#ifndef FILE_PANEL_H

#include "file-info.h"

typedef struct File_Panel {
    File_Infos file_infos;
    size_t offset;
    size_t select;
} File_Panel;

LUT_INCLUDE(T_File_Panel, t_file_panel, So, BY_REF, File_Panel, BY_REF);

void file_panel_free(File_Panel *panel);
void t_file_panel_ensure_exist(T_File_Panel *t, File_Panel **out, So path);

#define FILE_PANEL_H
#endif

