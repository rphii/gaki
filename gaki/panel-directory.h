#ifndef PANEL_DIRECTORY

#include "panel-file-info.h"

typedef struct Panel_Directory {
    Panel_File_Info *parent;
    Panel_File_Info *pwd;
    Panel_File_Infos *sub;
    size_t index;
    size_t offset;
} Panel_Directory;

#define PANEL_DIRECTORY
#endif

