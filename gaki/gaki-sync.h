#ifndef GAKI_SYNC_H

#include <pthread.h>
#include "panel-gaki.h"

typedef struct Gaki_Sync_Main {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    unsigned int update_do;
    unsigned int update_done;
    unsigned int render_do;
    unsigned int render_done;
} Gaki_Sync_Main;

typedef struct Gaki_Sync_Draw {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    unsigned int draw_do;
    unsigned int draw_skip;
    unsigned int draw_done;
    unsigned int draw_redraw;
} Gaki_Sync_Draw;

typedef struct Gaki_Sync_Input {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    unsigned int idle;
} Gaki_Sync_Input;

typedef struct Gaki_Sync_Panel {
    pthread_mutex_t mtx;
    Panel_Gaki panel_gaki;
    size_t count_register;
} Gaki_Sync_Panel;

typedef struct Gaki_Sync_T_File_Info {
    pthread_rwlock_t rwl;
    T_File_Info t_file_info;
} Gaki_Sync_T_File_Info;

#define GAKI_SYNC_H
#endif

