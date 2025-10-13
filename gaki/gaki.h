#ifndef GAKI_H

#include <rltui.h>
#include <rlpw.h>

#include "gaki-state.h"
#include "action.h"

typedef struct Gaki {
    struct timespec t0;
    struct timespec tE;
    Tui_Input input;
    Tui_Input input_prev;
    Tui_Screen screen;
    Tui_Buffer buffer;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    Pw pw;
    Pw pw_render;
    char *setbuf;
    bool resized;
    bool quit;
    bool drag;
    Gaki_State st;
    Action ac;

    pthread_cond_t render_cond;
    pthread_mutex_t render_mtx;
    bool render_do;
    bool render_busy;
    size_t frames;

} Gaki;

Gaki *gaki_global_get(void);
void gaki_global_set(Gaki *st);
void gaki_free(Gaki *gaki);

#define GAKI_H
#endif

