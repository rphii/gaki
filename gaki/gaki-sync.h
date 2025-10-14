#ifndef GAKI_SYNC_H

#include <pthread.h>

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
} Gaki_Sync_Draw;

#define GAKI_SYNC_H
#endif

