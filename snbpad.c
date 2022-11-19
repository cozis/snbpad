#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <raylib.h>
#include "gap.h"
#include "gapiter.h"
#include "utils.h"
#include "textdisplay.h"

GUIElement *focused = NULL;
GUIElement *last_focused = NULL;
GUIElement *elements[2]; 
size_t element_count = 0;

static void 
evaluateLeftBufferRegion(Rectangle *region, 
                         int win_w, int win_h)
{
    *region = (Rectangle) {
        .x = 10, 
        .y = 20, 
        .width  = (win_w - 30) / 2, 
        .height = win_h - 40,
    };
}

static void 
evaluateRightBufferRegion(Rectangle *region, 
                          int win_w, int win_h)
{
    *region = (Rectangle) {
        .x = (win_w - 30) / 2 + 20, 
        .y = 20, 
        .width  = (win_w - 30) / 2,
        .height = win_h - 40,
    };
}

static void updateElementRegions()
{
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    if (element_count == 1) {
        Rectangle region;
        region.x = 10;
        region.y = 20;
        region.width  = w - 20;
        region.height = h - 40;
        elements[0]->region = region;
    } else {
        assert(element_count == 2);
        evaluateLeftBufferRegion(&elements[0]->region, w, h);
        evaluateRightBufferRegion(&elements[1]->region, w, h);
    }
}

static void switchToSingleBufferMode(bool remove_right_buffer)
{
    assert(element_count == 2);
    GUIElement *removed;
    if (remove_right_buffer)
        removed = elements[1];
    else {
        removed = elements[0];
        elements[0] = elements[1];
    }
    element_count = 1;
    GUIElement_free(removed);
    if (focused == removed)
        focused = NULL;
    if (last_focused == removed)
        last_focused = NULL;
    updateElementRegions();
}

static void switchToDoubleBufferMode(const TextDisplayStyle *style, 
                                     bool new_buffer_to_the_right)
{
    assert(element_count == 1);
    
    Rectangle region = {0, 0, 0, 0}; // Anything goes.
    GUIElement *elem = TextDisplay_new(region, "First-buffer", NULL, style);
    if (elem != NULL) {

        if (new_buffer_to_the_right)
            elements[1] = elem;
        else {
            elements[1] = elements[0];
            elements[0] = elem;
        }
        element_count = 2;
        updateElementRegions();
    }
}

void snbpad(void)
{
    int w = 800;
    int h = 700;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(w, h, "SnBpad");
    if (!IsWindowReady()) {
        TraceLog(LOG_FATAL, "Failed to create window");
        return;
    }

    const char *font_file = "/usr/share/fonts/TTF/Inconsolata-Medium.ttf";

    TextDisplayStyle style = {
        .lineno = {
            .hide = false,
            .nobg = false,
            .fgcolor = {0xcc, 0xcc, 0xcc, 0xff},
            .bgcolor = {0x33, 0x33, 0x33, 0xff},
            .font_file = font_file,
            .font_size = 30,
            .auto_width = false,
            .width = 60,
            .h_align = TextAlignH_RIGHT,
            .v_align = TextAlignV_CENTER,
            .padding_up = 3,
            .padding_down = 3,
            .padding_left = 10,
            .padding_right = 10,
        },
        .text = {
            .nobg = false,
            .font_file = font_file,
            .font_size = 30,
            .v_align = TextAlignV_CENTER,
            .bgcolor = {48, 56, 65, 255},
            .fgcolor = {0xee, 0xee, 0xee, 0xff},
            .selection_bgcolor = {0xcc, 0xcc, 0xff, 0xff},
        },
        .cursor = {
            .blink_period = 500, 
            .bgcolor = {0xbb, 0xbb, 0xbb, 0xff},
        },
        .scroll = {
            .inertia = 10,
        },
        .scrollbar_size = 20,
        .auto_line_height = true,
        .line_height = 20,
    };

    {
        Rectangle region = {0, 0, 0, 0}; // Anything goes.
        GUIElement *tdisplay = TextDisplay_new(region, "First-buffer", NULL, &style);
        if (tdisplay == NULL)
            return;
        
        elements[element_count++] = tdisplay;
        updateElementRegions();
    }

    int arrow_press_interval = 70;

    SetTraceLogLevel(LOG_DEBUG);

    const int fps = 60;
    const int ms_per_frame = 1000 / fps;
    bool mouse_button_left_was_pressed = false;
    bool arrow_left_was_pressed = false;
    bool arrow_right_was_pressed = false;
    int left_arrow_counter = 0;
    int right_arrow_counter = 0;
    SetTargetFPS(fps);
    uint64_t time_in_ms = 0;

    typedef enum {
        NO_CHANGE,
        CHANGE_LEFT,
        CHANGE_RIGHT,
    } PendingModeChange; 
    
    while (!WindowShouldClose()) {

        PendingModeChange pending = NO_CHANGE;

        for (size_t i = 0; i < element_count; i++)
            GUIElement_tick(elements[i], time_in_ms);
        time_in_ms += ms_per_frame;

        if (IsWindowResized())
            updateElementRegions();
        
        GUIElement *hovered = NULL;

        Vector2 cursor_point = {GetMouseX(), GetMouseY()};
        for (size_t i = 0; i < element_count && hovered == NULL; i++)
            if (CheckCollisionPointRec(cursor_point, elements[i]->region))
                hovered = elements[i];

        if (hovered != NULL) {

            Vector2 motion = GetMouseWheelMoveV();
            GUIElement_onMouseWheel(hovered, 5 * motion.y);
        }

        for (size_t i = 0; i < element_count; i++)
            GUIElement_onMouseMotion(elements[i], 
                cursor_point.x - elements[i]->region.x, 
                cursor_point.y - elements[i]->region.y);

        bool mouse_button_left_is_pressed = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (mouse_button_left_is_pressed && !mouse_button_left_was_pressed) {
            
            if (hovered != NULL) {
                GUIElement_onClickDown(hovered, 
                    cursor_point.x - hovered->region.x, 
                    cursor_point.y - hovered->region.y);
                if (hovered != focused) {
                    if (GUIElement_allowsFocus(hovered)) {
                        if (focused == NULL) {
                            TraceLog(LOG_INFO, "Element [%s] gained focus", 
                                     hovered->name);
                        } else {
                            GUIElement_onFocusLost(focused);
                            TraceLog(LOG_INFO, "Focus changed from element [%s] to [%s]", 
                                     focused->name, hovered->name);
                        }
                        GUIElement_onFocusGained(hovered);
                        last_focused = hovered;
                        focused = hovered;
                    } else {
                        GUIElement_onFocusLost(focused);
                        TraceLog(LOG_INFO, "Element [%s] lost focus", 
                                 focused->name);
                        SetWindowTitle("SnBpad"); 
                        focused = NULL;
                    }
                }
            } else {
                if (focused != NULL) {
                    GUIElement_onFocusLost(focused);
                    TraceLog(LOG_INFO, "Element [%s] lost focus", 
                             focused->name);
                    focused = NULL;
                }
            }

            for (size_t i = 0; i < element_count; i++)
                if (elements[i] != hovered)
                    GUIElement_offClickDown(elements[i]);

        } else if (!mouse_button_left_is_pressed && mouse_button_left_was_pressed) {
            for (size_t i = 0; i < element_count; i++) {
                Vector2 fake = {
                    .x = cursor_point.x - elements[i]->region.x,
                    .y = cursor_point.y - elements[i]->region.y,
                };
                if (elements[i] != hovered) {
                    fake.x = MAX(fake.x, 0);
                    fake.x = MIN(fake.x, elements[i]->region.width);
                    fake.y = MAX(fake.y, 0);
                    fake.y = MIN(fake.y, elements[i]->region.height);
                }
                GUIElement_clickUp(elements[i], fake.x, fake.y);
            }
        }
        mouse_button_left_was_pressed = mouse_button_left_is_pressed;

        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            
            if (last_focused != NULL) {
                if (IsKeyPressed(KEY_S))
                    GUIElement_onSave(last_focused);
            }
            
            if (focused != NULL) {

                if (IsKeyPressed(KEY_O))
                    GUIElement_onOpen(focused);

                if (IsKeyPressed(KEY_C))
                    GUIElement_onCopy(focused);
                
                if (IsKeyPressed(KEY_X))
                    GUIElement_onCut(focused);
                
                if (IsKeyPressed(KEY_V))
                    GUIElement_onPaste(focused);

                if (IsKeyPressed(KEY_LEFT))
                    pending = CHANGE_LEFT;

                if (IsKeyPressed(KEY_RIGHT))
                    pending = CHANGE_RIGHT;
            }
        
        } else {

            bool trigger_left_arrow;
            bool arrow_left_is_pressed = IsKeyDown(KEY_LEFT);
            if (arrow_left_is_pressed) {
                if (!arrow_left_was_pressed) {
                    trigger_left_arrow = true;
                    left_arrow_counter = 0;
                } else {
                    if (left_arrow_counter * ms_per_frame > arrow_press_interval) {
                        trigger_left_arrow = true;
                        left_arrow_counter = 0;
                    } else {
                        trigger_left_arrow = false;
                        left_arrow_counter++;
                    }
                }
            } else
                trigger_left_arrow = false;
            arrow_left_was_pressed = arrow_left_is_pressed;

            bool trigger_right_arrow;
            bool arrow_right_is_pressed = IsKeyDown(KEY_RIGHT);
            if (arrow_right_is_pressed) {
                if (!arrow_right_was_pressed) {
                    trigger_right_arrow = true;
                    right_arrow_counter = 0;
                } else {
                    if (right_arrow_counter * ms_per_frame > arrow_press_interval) {
                        trigger_right_arrow = true;
                        right_arrow_counter = 0;
                    } else {
                        trigger_right_arrow = false;
                        right_arrow_counter++;
                    }
                }
            } else
                trigger_right_arrow = false;
            arrow_right_was_pressed = arrow_right_is_pressed;

            if (focused != NULL) {

                if (trigger_left_arrow)
                    GUIElement_onArrowLeftDown(focused);

                if (trigger_right_arrow)
                    GUIElement_onArrowRightDown(focused);

                if (IsKeyPressed(KEY_ENTER))
                    GUIElement_onReturnDown(focused);
            
                if (IsKeyPressed(KEY_BACKSPACE))
                    GUIElement_onBackspaceDown(focused);
            
                int key = GetCharPressed();
                while (key > 0) {
                    GUIElement_onTextInput2(focused, key);
                    key = GetCharPressed();
                }
            }
        }

        SetTraceLogLevel(LOG_WARNING);

        RenderTexture2D targets[32];
        size_t target_count = 0;

        for (size_t i = 0; i < element_count; i++) {
            
            GUIElement *elem = elements[i];
            Rectangle region = elem->region;

            RenderTexture2D target = LoadRenderTexture(region.width, region.height);
            BeginTextureMode(target);
            GUIElement_draw(elem);
            EndTextureMode();

            targets[target_count++] = target;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        for (size_t i = 0; i < element_count; i++) {
            
            GUIElement *elem = elements[i];
            Rectangle region = elem->region;

            Rectangle src, dst;
            src.x = 0;
            src.y = 0;
            src.width  =  region.width;
            src.height = -region.height;
            dst.x = region.x;
            dst.y = region.y;
            dst.width  = region.width;
            dst.height = region.height;
            Vector2 org = {0, 0};
            DrawTexturePro(targets[i].texture, src, 
                           dst, org, 0, WHITE);
        }
        EndDrawing();

        for (size_t i = 0; i < element_count; i++)
            UnloadRenderTexture(targets[i]);

        SetTraceLogLevel(LOG_DEBUG);
    
        if (pending == CHANGE_LEFT) {
            if (element_count == 1)
                switchToDoubleBufferMode(&style, false);
            else
                switchToSingleBufferMode(false);
        } else if (pending == CHANGE_RIGHT) {
            if (element_count == 1)
                switchToDoubleBufferMode(&style, true);
            else
                switchToSingleBufferMode(true);
        }
        pending = NO_CHANGE;
    }
    for (size_t i = 0; i < element_count; i++)
        GUIElement_free(elements[i]);
    CloseWindow();
}

int main(void)
{
    snbpad();
    return 0;
}
