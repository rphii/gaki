#include "panel-input.h"
#include "gaki.h"

void panel_input_update(Panel_Input *panel) {
    if(!panel->visible) return;
    ASSERT_ARG(panel);
    ASSERT_ARG(panel->mtx);
    ASSERT_ARG(panel->text);
    Panel_Input_Config cfg = panel->config;
    Panel_Input_Layout *ly = &panel->layout;
    ly->rc = *cfg.rc;
    //ly->rc.dim.y = 1;
    //ly->rc.dim.x = panel->text->visual_len;
}

bool panel_input_input(Panel_Input *panel, Tui_Sync_Main *sync_m, Tui_Input *input, bool *flush) {
    ASSERT_ARG(panel);
    ASSERT_ARG(panel->mtx);
    ASSERT_ARG(panel->text);
    ASSERT_ARG(panel->visible);

    bool update = false;
    pthread_mutex_lock(panel->mtx);
    switch(input->id) {
        case INPUT_CODE: {
            switch(input->code) {
                case KEY_CODE_ESC: {
                    tui_text_line_clear(panel->text);
                    panel->visible = false;
                    panel->config = (Panel_Input_Config){0};
                    update = true;
                } break;
                case KEY_CODE_ENTER: {
                    panel->visible = false;
                    panel->config = (Panel_Input_Config){0};
                    update = true;
                } break;
                case KEY_CODE_BACKSPACE: {
                    tui_text_line_pop(panel->text);
                    update = true;
                } break;
                default: break;
            }
        } break;
        case INPUT_TEXT: {
            tui_text_line_push(panel->text, input->text);
            update = true;
        } break;
        default: break;
    }
    pthread_mutex_unlock(panel->mtx);
    return update;
}

void panel_input_render(Panel_Input *panel, Tui_Buffer *buffer) {
    if(!panel->visible) return;

    ASSERT_ARG(panel);
    ASSERT_ARG(panel->mtx);
    ASSERT_ARG(panel->text);
    ASSERT_ARG(panel->visible);

    pthread_mutex_lock(panel->mtx);

    //printff("\r %u %u %u %u",panel->layout.rc.anc.x,panel->layout.rc.anc.x,panel->layout.rc.dim.y,panel->layout.rc.dim.y);
    Tui_Text_Line tmp = {0};
    tui_text_line_fmt(&tmp, "%.*s%.*s", SO_F(panel->config.prompt), SO_F(panel->text->so));
    tui_buffer_draw(buffer, panel->layout.rc, 0, 0, 0, tmp.so);

    buffer->cursor.pt = panel->layout.rc.anc;
    buffer->cursor.pt.x += tmp.visual_len;
    buffer->cursor.id = TUI_CURSOR_BAR;

    pthread_mutex_unlock(panel->mtx);
    so_free(&tmp.so);
}


