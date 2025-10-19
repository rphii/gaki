#ifndef PANEL_FILE_INFO

#include "file-info.h"

typedef struct Panel_File_Info {
    File_Info *ref;
    size_t offset;
    bool printable;
    bool selected;
    bool have_read;
} Panel_File_Info, *Panel_File_Infos;

#define PANEL_FILE_INFO
#endif

