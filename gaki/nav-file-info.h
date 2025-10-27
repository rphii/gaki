#ifndef NAV_FILE_INFO

#include "file-info.h"

typedef struct Nav_File_Info {
    File_Info *ref;
    pthread_mutex_t mtx;
    bool selected;
    bool have_read;
} Nav_File_Info, *Nav_File_Infos;

#define NAV_FILE_INFO
#endif

