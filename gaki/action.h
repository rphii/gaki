#ifndef ACTION_H

#include <stdbool.h>
#include <sys/types.h>
#include <rltui/tui-point.h>

typedef struct Action {
    ssize_t select_up;
    ssize_t select_down;
    ssize_t select_left;
    ssize_t select_right;
    bool quit;
} Action;

typedef enum {
    ACTION_NONE,
    /* stuff below */
    ACTION_MOUSE_DOWN_LEFT,
    ACTION_MOUSE_DOWN_RIGHT,
    ACTION_MOUSE_DOWN_MIDDLE,
    ACTION_MOUSE_UP_LEFT,
    ACTION_MOUSE_UP_RIGHT,
    ACTION_MOUSE_UP_MIDDLE,
    ACTION_MOUSE_MOVE,
    ACTION_SELECT_UP,
    ACTION_SELECT_DOWN,
    ACTION_SELECT_LEFT,
    ACTION_SELECT_RIGHT,
    /* stuff above */
    ACTION__COUNT
} Action_List;

typedef enum {
    ACTION_DATA_NONE,
    ACTION_DATA_INT,
    ACTION_DATA_POINT,
} Action_Data_List;

typedef struct Action_Data {
    Action_Data_List id;
    union {
        int num;
        Tui_Point point;
    };
} Action_Data;

typedef struct Action2 {
    Action_List id;
    Action_Data data;
} Action2, *Actions2;

#define ACTION_H
#endif

