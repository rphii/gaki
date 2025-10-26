#ifndef GAKI_SYNC_H

#include <pthread.h>
#include "panel-gaki.h"

typedef struct Gaki_Sync_Panel {
    pthread_mutex_t mtx;
    Panel_Gaki panel_gaki;
    size_t count_register;
} Gaki_Sync_Panel;

typedef struct Gaki_Sync_T_File_Info {
    pthread_rwlock_t rwl;
    T_File_Info t_file_info;
} Gaki_Sync_T_File_Info;

#define MTX_DO(mtx) \
        pthread_mutex_lock(mtx); \
        do { \

#define MTX_BREAK \
        break

#define MTX_DONE(mtx) \
        } while(0); \
        pthread_mutex_unlock(mtx)

#define GAKI_SYNC_H
#endif

