#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <raylib.h>
#include "gap.h"
#include "gapiter.h"
#include "utils.h"
#include "textdisplay.h"

void evaluateLeftBufferRegion(Rectangle *region, 
                              int win_w, int win_h)
{
    *region = (Rectangle) {
        .x = 10, 
        .y = 20, 
        .width  = (win_w - 30) / 2, 
        .height = win_h - 40,
    };
}

void evaluateRightBufferRegion(Rectangle *region, 
                               int win_w, int win_h)
{
    *region = (Rectangle) {
        .x = (win_w - 30) / 2 + 20, 
        .y = 20, 
        .width  = (win_w - 30) / 2,
        .height = win_h - 40,
    };
}

void snbpad()
{
    int w = 300;
    int h = 400;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(w, h, "SnBpad");
    if (!IsWindowReady()) {
        TraceLog(LOG_FATAL, "Failed to create window");
        return;
    }

    const char *font_file = "/usr/share/fonts/TTF/Inconsolata-Medium.ttf";
    Font font = LoadFont(font_file);

    TextDisplayStyle style = {
        .lineno = {
            .hide = false,
            .nobg = false,
            .fgcolor = {0xcc, 0xcc, 0xcc, 0xff},
            .bgcolor = {0x33, 0x33, 0x33, 0xff},
            .font = &font,
            .auto_width = false,
            .width = 60,
            .h_align = TextAlignH_RIGHT,
            .v_align = TextAlignV_CENTER,
            .padding_up = 0,
            .padding_down = 0,
            .padding_left = 10,
            .padding_right = 10,
        },
        .text = {
            .nobg = false,
            .font = &font,
            .bgcolor = {0xff, 0xff, 0xff, 0xff},
            .fgcolor = {0x33, 0x33, 0x33, 0xff},
            .selection_bgcolor = {0xcc, 0xcc, 0xff, 0xff},
        },
        .cursor = {
            .bgcolor = {0xbb, 0xbb, 0xbb, 0xff},
        },
        .scroll = {
            .inertia = 10,
        },
        .scrollbar_size = 20,
        .auto_line_height = true,
    };

    GUIElement *elements[32]; 
    size_t element_count = 0;

    {
        Rectangle region;
        evaluateLeftBufferRegion(&region, w, h);
        GUIElement *tdisplay = TextDisplay_new(region, "Left-buffer", NULL, &style);
        if (tdisplay == NULL)
            return;
        
        elements[element_count++] = tdisplay;
    }

    {
        Rectangle region;
        evaluateRightBufferRegion(&region, w, h);
        GUIElement *tdisplay = TextDisplay_new(region, "Right-Buffer", NULL, &style);
        if (tdisplay == NULL)
            return;
        
        elements[element_count++] = tdisplay;
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

    GUIElement *focused = NULL;
    GUIElement *last_focused = NULL;
    while (!WindowShouldClose()) {

        for (size_t i = 0; i < element_count; i++)
            GUIElement_tick(elements[i]);

        if (IsWindowResized()) {
            w = GetScreenWidth();
            h = GetScreenHeight();
            evaluateLeftBufferRegion(&elements[0]->region, w, h);
            evaluateRightBufferRegion(&elements[1]->region, w, h);
        }

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
                            TraceLog(LOG_INFO, "Focus changed from element [%s] to [%s]", 
                                     focused->name, hovered->name);
                        }
                        last_focused = hovered;
                        focused = hovered;
                    } else {
                        TraceLog(LOG_INFO, "Element [%s] lost focus", 
                                 focused->name);
                        focused = NULL;
                    }
                }
            } else {
                if (focused != NULL) {
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
    }
    for (size_t i = 0; i < element_count; i++)
        GUIElement_free(elements[i]);
    UnloadFont(font);
    CloseWindow();
}

int main(int argc, char **argv)
{
    const char *file;
    if (argc > 1)
        file = argv[1];
    else
        file = NULL;

    snbpad(file, -1, -1);
    return 0;
}