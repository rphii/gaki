#ifndef FILE_INFOS_H

#include <rlso.h>
#include <rltui.h>
#include <sys/stat.h>

typedef struct File_Info File_Info;
typedef struct File_Info *File_Infos;
typedef struct Gaki_Sync_T_File_Info Gaki_Sync_T_File_Info;

//VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, BASE);
//VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, SORT);
LUT_INCLUDE(T_File_Info, t_file_info, So, BY_VAL, File_Info, BY_REF);

typedef enum {
    FILE_GRAPHIC_NONE,
    FILE_GRAPHIC_UNICODE,
    FILE_GRAPHIC_KITTY,
} File_Graphic_List;

typedef struct Gaki_Image {
    unsigned int w;
    unsigned int h;
    unsigned int ch;
    unsigned char *data;
} Gaki_Image;

typedef struct File_Graphic {
    File_Graphic_List id;
    Gaki_Image thumb;
    So cvt_str;
    Tui_Point cvt_rct;
} File_Graphic;

typedef enum File_Content_List {
    FILE_CONTENT_NONE,
    FILE_CONTENT_TEXT,
    FILE_CONTENT_DIRECTORY,
    FILE_CONTENT_GRAPH,
} File_Content_List;

typedef struct File_Content {
    union {
        So text;
        File_Infos *files;
        File_Graphic graphic;
    };
    File_Content_List id;
} File_Content;

typedef struct File_Info {
    So path;
    So_Filesig_List signature_id;
    bool signature_unsure;
    struct stat stats;
    File_Content content;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    bool loaded;
    bool loaded_done;
} File_Info, *File_Infos;

int file_info_cmp(File_Info *a, File_Info *b);
void file_info_free(File_Info *a);
size_t file_info_rel(File_Info *info);
double file_info_relsize(File_Info *info);
char *file_info_relcstr(File_Info *info);

File_Info *file_info_ensure(Gaki_Sync_T_File_Info *sync, So path);

#define FILE_INFOS_H
#endif

