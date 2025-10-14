#ifndef ACTION_H

#include <sys/types.h>

typedef struct Action {
    ssize_t select_up;
    ssize_t select_down;
    ssize_t select_left;
    ssize_t select_right;
} Action;


#define ACTION_H
#endif

