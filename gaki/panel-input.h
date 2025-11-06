#ifndef PANEL_INPUT_H

#include <rlso.h>
#include <rltui.h>

typedef struct Panel_Input_Config {
    So prompt;
    Tui_Rect *rc;
} Panel_Input_Config;

typedef struct Panel_Input_Layout {
    Tui_Rect rc;
} Panel_Input_Layout;

typedef struct Panel_Input {
    Panel_Input_Config config;
    Panel_Input_Layout layout;
    Tui_Text_Line *text;
    pthread_mutex_t *mtx;
    bool visible;
} Panel_Input;

void panel_input_update(Panel_Input *panel);
bool panel_input_input(Panel_Input *panel, Tui_Sync_Main *sync_m, Tui_Input *input, bool *flush);
void panel_input_render(Panel_Input *panel, Tui_Buffer *buffer);

#define PANEL_INPUT_H
#endif // PANEL_INPUT_H

