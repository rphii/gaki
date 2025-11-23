#ifndef NAV_DIRECTORY

#include <rlpw.h>
#include "nav-file-info.h"

typedef struct Tui_Sync_Main Tui_Sync_Main;
typedef struct Gaki_Sync_Panel Gaki_Sync_Panel;
typedef struct Gaki_Sync_T_File_Info Gaki_Sync_T_File_Info;
typedef struct Nav_Directory Nav_Directory;
typedef struct Nav_Directory *Nav_Directories;

typedef struct Nav_Directory {
    Nav_File_Info pwd;
    Nav_Directory *parent;
    Nav_Directories *list;
    Tui_Text_Line filter;
    Tui_Text_Line search;
    size_t index;
    size_t offset;
} Nav_Directory, *Nav_Directories;

void nav_directory_select_up(Nav_Directory *nav, bool show_dots, size_t n);
void nav_directory_select_down(Nav_Directory *nav, bool show_dots, size_t n);
void nav_directory_select_at(Nav_Directory *nav, bool show_dots, size_t i);
void nav_directory_select_any_next_visible(Nav_Directory *nav, bool show_dots);
void nav_directory_select_any_prev_visible(Nav_Directory *nav, bool show_dots);
void nav_directory_offset_center(Nav_Directory *nav, bool show_dots, Tui_Point dim);

void nav_directory_search_next(Nav_Directory *nav, bool show_dots, size_t index, So search);
void nav_directory_search_prev(Nav_Directory *nav, bool show_dots, size_t index, So search);

void nav_directory_dispatch_readdir(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir, Nav_Directory *child);
void nav_directory_dispatch_readreg(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir);
void nav_directory_dispatch_readany(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, Nav_Directory *dir);
void nav_directory_dispatch_register(Pw *pw, Tui_Sync_Main *sync_m, Gaki_Sync_T_File_Info *sync_t, Gaki_Sync_Panel *sync, So path);

bool nav_directory_visible_check(Nav_Directory *nav, bool show_dots, So filter);
size_t nav_directory_visible_count(Nav_Directory *nav, bool show_dots);

#define NAV_DIRECTORY
#endif

