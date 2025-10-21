#ifndef NAV_DIRECTORY

#include <rlpw.h>
#include "nav-file-info.h"

typedef struct Gaki_Sync_Main Gaki_Sync_Main;
typedef struct Gaki_Sync_Panel Gaki_Sync_Panel;
typedef struct Gaki_Sync_T_File_Info Gaki_Sync_T_File_Info;
typedef struct Nav_Directory Nav_Directory;
typedef struct Nav_Directory *Nav_Directories;

typedef struct Nav_Directory {
    Nav_File_Info pwd;
    Nav_Directory *parent;
    Nav_Directories *list;
    size_t index;
    size_t offset;
} Nav_Directory, *Nav_Directories;

void nav_directory_dispatch_register(Pw *pw, Gaki_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, So path);

#define NAV_DIRECTORY
#endif

