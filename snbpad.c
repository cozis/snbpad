#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <raylib.h>
#include "gap.h"
#include "gapiter.h"
#include "utils.h"
#include "treeview.h"
#include "splitview.h"
#include "textdisplay.h"

#include "font_data_inconsolata_light.c"
#include "font_data_inconsolata_medium.c"

GUIElement *focused = NULL;
GUIElement *last_focused = NULL;
GUIElement *elements[2]; 
size_t element_count = 0;

static void treeViewCallback(const char *file, 
                             size_t file_len, 
                             void *userp)
{
    fprintf(stderr, "Opening [%s]\n", file);
    if (last_focused != NULL)
        GUIElement_openFile(last_focused, file);
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

    ScrollbarStyle scrollbar_style = {
        .size = 20,
        .inertia = 10,
    };

    ScrollbarStyle tree_view_scrollbar_style = {
        .size = 10,
        .inertia = 10,
    };

    TextDisplayStyle style = {
        .lineno = {
            .hide = false,
            .nobg = false,
            .fgcolor = {0xcc, 0xcc, 0xcc, 0xff},
            .bgcolor = {48, 56, 65, 255},//{0x33, 0x33, 0x33, 0xff},
            .font_file = NULL,
            .font_data = font_data_inconsolata_medium,
            .font_data_size = sizeof(font_data_inconsolata_medium),
            .font_size = 22,
            .auto_width = true,
            //.width = 40,
            .h_align = TextAlignH_RIGHT,
            .v_align = TextAlignV_CENTER,
            .padding_up = 3,
            .padding_down = 3,
            .padding_left = 10,
            .padding_right = 10,
        },
        .text = {
            .nobg = false,
            .font_file = NULL,
            .font_data = font_data_inconsolata_medium,
            .font_data_size = sizeof(font_data_inconsolata_medium),
            .font_size = 22,
            .v_align = TextAlignV_CENTER,
            .bgcolor = {48, 56, 65, 255},
            .fgcolor = {0xee, 0xee, 0xee, 0xff},
            .selection_bgcolor = {87, 95, 104, 0xff},
        },
        .cursor = {
            .bgcolor = {0xbb, 0xbb, 0xbb, 0xff},
        },
        .v_scroll = &scrollbar_style,
        .h_scroll = &scrollbar_style,
        .auto_line_height = true,
        .line_height = 20,
    };

    SplitViewStyle split_view_style = {
        .separator = { .size = 15, },
        .resize_mode = SplitResizeMode_KEEPRATIOS,
        .bgcolor = {0x33, 0x33, 0x33, 0xff},
    };

    SplitViewStyle tree_split_view_style = {
        .separator = { .size = 15, },
        .resize_mode = SplitResizeMode_RESIZERIGHT,
        .bgcolor = {0x33, 0x33, 0x33, 0xff},
    };

    TreeViewStyle tree_view_style = {
        .bgcolor = {0x33, 0x33, 0x33, 0xff},
        .fgcolor = {0xcc, 0xcc, 0xcc, 0xff},
        .font_file = NULL,
        .font_data = font_data_inconsolata_light,
        .font_data_size = sizeof(font_data_inconsolata_light),
        .font_size = 23,
        .auto_line_height = false,
        .line_height = 30,
        .padding_top = 10,
        .padding_left = 10,
        .subtree_padding_left = 20,
        .v_scroll = &tree_view_scrollbar_style,
        .h_scroll = &tree_view_scrollbar_style,
    };

    GUIElement *sv2;
    {
        GUIElement *sv;
        {
            GUIElement *ltd;
            {
                Rectangle region = {0, 0, 0, 0}; // Anything goes.
                ltd = TextDisplay_new(region, "Left-Text-Display", NULL, &style);
                if (ltd == NULL)
                    return;
            }

            GUIElement *rtd;
            {
                Rectangle region = {0, 0, 0, 0}; // Anything goes.
                rtd = TextDisplay_new(region, "Right-Text-Display", NULL, &style);
                if (rtd == NULL) {
                    GUIElement_free(ltd);
                    return;
                }
            }
            
            Rectangle region = {0, 0, 0, 0};
            sv = SplitView_new(region, "Split-View",
                               ltd, rtd, SplitDirection_HORIZONTAL,
                               &split_view_style);
            if (sv == NULL) {
                GUIElement_free(ltd);
                GUIElement_free(rtd);
                return;
            }
        }

        GUIElement *tv;
        {
            Rectangle region = {0};
            char tree_view_path[1024];
            if (getcwd(tree_view_path, sizeof(tree_view_path)) == NULL)
                strcpy(tree_view_path, ".");
            tv = TreeView_new(region, "Tree-View", 
                              tree_view_path, 
                              treeViewCallback,
                              &tree_view_style,
                              NULL);
            if (tv == NULL) {
                GUIElement_free(sv);
                return;
            }
        }
        Rectangle region = {
            .x = 0,
            .y = 0,
            .width = w,
            .height = h,
        };
        sv2 = SplitView_new(region, "Split-View-2",
                            tv, sv, SplitDirection_HORIZONTAL,
                            &tree_split_view_style);
        if (sv2 == NULL) {
            GUIElement_free(sv);
            GUIElement_free(tv);
            return;
        }
    }
    elements[element_count++] = sv2;

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
    while (!WindowShouldClose()) {

        {
            int min_w = 0;
            int min_h = 0;
            for (size_t i = 0; i < element_count; i++) {
                int elem_w;
                int elem_h;
                GUIElement_getMinimumSize(elements[i], &elem_w, &elem_h);
                min_w = MAX(min_w, elem_w);
                min_h = MAX(min_h, elem_h);
            }
            SetWindowMinSize(min_w, min_h);
        }

        for (size_t i = 0; i < element_count; i++)
            GUIElement_tick(elements[i], time_in_ms);
        time_in_ms += ms_per_frame;

        if (IsWindowResized()) {
            GUIElement_setRegion(sv2, (Rectangle) {
                .width = GetScreenWidth(),
                .height = GetScreenHeight(),
                .x = 0, .y = 0,
            });
        }
        
        GUIElement *hovered = NULL;

        Vector2 cursor_point = {GetMouseX(), GetMouseY()};
        for (size_t i = 0; i < element_count && hovered == NULL; i++)
            if (CheckCollisionPointRec(cursor_point, elements[i]->region))
                hovered = GUIElement_getHovered(elements[i], 
                            cursor_point.x - elements[i]->region.x, 
                            cursor_point.y - elements[i]->region.y);

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
                GUIElement *new_focused = GUIElement_onClickDown(hovered, 
                    cursor_point.x - hovered->region.x, 
                    cursor_point.y - hovered->region.y);
                
                if (new_focused == NULL) {

                    if (focused != NULL) {
                        GUIElement_onFocusLost(focused);
                        TraceLog(LOG_INFO, "Element [%s] lost focus", 
                                 focused->name);
                        focused = NULL;
                    }

                } else if (new_focused != focused) {
                    
                    if (focused == NULL) {
                        TraceLog(LOG_INFO, "Element [%s] gained focus", 
                                 new_focused->name);
                    } else {
                        GUIElement_onFocusLost(focused);
                        TraceLog(LOG_INFO, "Focus changed from element [%s] to [%s]", 
                                 focused->name, new_focused->name);
                    }
                    GUIElement_onFocusGained(new_focused);
                    last_focused = new_focused;
                    focused = new_focused;
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

        BeginDrawing();
        ClearBackground(RAYWHITE);
        for (size_t i = 0; i < element_count; i++)
            GUIElement_draw(elements[i]);
        /*
        if (hovered != NULL)
            DrawRectangleLines(hovered->region.x + 5,
                               hovered->region.y + 5,
                               hovered->region.width - 10,
                               hovered->region.height - 10,
                               PURPLE);
        */        
        EndDrawing();
        SetTraceLogLevel(LOG_DEBUG);
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
