#ifndef FILE_INFOS_H

#include <rlso.h>
#include <sys/stat.h>

typedef struct File_Info File_Info;
typedef struct File_Info *File_Infos;
typedef struct Gaki_Sync_T_File_Info Gaki_Sync_T_File_Info;

//VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, BASE);
//VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, SORT);
LUT_INCLUDE(T_File_Info, t_file_info, So, BY_VAL, File_Info, BY_REF);

typedef struct File_Info {
    So path;
    struct stat stats;
    union {
        So text;
        File_Infos *files;
    } content;
    bool loaded;
} File_Info, *File_Infos;

int file_info_cmp(File_Info *a, File_Info *b);
void file_info_free(File_Info *a);
size_t file_info_rel(File_Info *info);
double file_info_relsize(File_Info *info);
char *file_info_relcstr(File_Info *info);

File_Info *file_info_ensure(Gaki_Sync_T_File_Info *sync, So path);

#define FILE_INFOS_H
#endif

