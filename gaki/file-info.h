#ifndef FILE_INFOS_H

#include <rlso.h>
#include <sys/stat.h>

typedef struct File_Info {
    bool printable;
    So filename;
    So path;
    union {
        So content;
        struct File_Panel *file_panel;
    };
    struct stat stats;
    size_t offset;
    bool selected;
} File_Info;

VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, BASE);
VEC_INCLUDE(File_Infos, file_infos, File_Info, BY_REF, SORT);

int file_info_cmp(File_Info *a, File_Info *b);
void file_info_free(File_Info *a);
size_t file_info_rel(File_Info *info);
double file_info_relsize(File_Info *info);
char *file_info_relcstr(File_Info *info);

#define FILE_INFOS_H
#endif

