#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <raylib.h>
#include "gap.h"
#include "gapiter.h"
#include "utils.h"
#include "textdisplay.h"

static void save(const char *file, 
                 GapBuffer *buffer)
{
    FILE *stream = fopen(file, "wb");
    if (stream == NULL) {
        TraceLog(LOG_WARNING, "Failed to open \"%s\" in write mode\n", file);
    } else {
        if (!GapBuffer_saveToStream(buffer, stream))
            TraceLog(LOG_WARNING, "Failed to save to \"%s\"\n", file);
        fclose(stream);
    }
}

static bool determineFileName(char *dst, size_t dstmax)
{
    int attempt = -1;
    do {
        attempt++;
        int k = snprintf(dst, dstmax, "unnamed-%d.txt", attempt);
        if (k < 0 || (size_t) k >= dstmax)
            return false;
    } while (FileExists(dst));
    return true;
}

void snbpad(const char *file, int w, int h)
{
    bool file_doesnt_exist_yet;

    char file_fallback[256];
    if (file == NULL) {
        if (!determineFileName(file_fallback, sizeof(file_fallback))) {
            TraceLog(LOG_ERROR, "Couldn't determine a valid file name");
            return;
        }
        file = file_fallback;
        file_doesnt_exist_yet = true;
    } else
        file_doesnt_exist_yet = !FileExists(file);

    if (w < 0) w = 300;
    if (h < 0) h = 400;

    char caption[256];
    snprintf(caption, sizeof(caption), 
             "SnBpad - %s", file);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(w, h, caption);
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "Failed to create window");
        return;
    }

    const char *font_file = "/usr/share/fonts/TTF/Inconsolata-Medium.ttf";
    Font font = LoadFont(font_file);

    GapBuffer buffer;
    if (file_doesnt_exist_yet)
        GapBuffer_initEmpty(&buffer);
    else {
        if (!GapBuffer_initFile(&buffer, file)) {
            TraceLog(LOG_WARNING, "Failed to load \"%s\"", file);
            GapBuffer_initEmpty(&buffer);
        }
    }

    TextDisplay tdisp = {
        .x = 0,
        .y = 0,
        .w = w,
        .h = h,
        .lineno = {
            .hide = false,
            .nobg = true,
            .fgcolor = {0xcc, 0xcc, 0xcc, 0xff},
            .bgcolor = {0x33, 0x33, 0x33, 0xff},
            .font = &font,
            .auto_width = true,
            .width = 60,
            .h_align = TextAlignH_RIGHT,
            .v_align = TextAlignV_CENTER,
            .padding_up = 0,
            .padding_down = 0,
            .padding_left = 10,
            .padding_right = 10,
        },
        .text = {
            .nobg = true,
            .font = &font,
            .fgcolor = {0x00, 0x00, 0x00, 0xff},
            .selection_bgcolor = {0xcc, 0xcc, 0xff, 0xff},
        },
        .cursor = {
            .bgcolor = {0x00, 0x00, 0x00, 0xff},
        },
        .v_scroll = {
            .inertia = 10,
            .force = 0,
            .amount = 0,
            .start_amount = 0,
            .active = false,
            .start_cursor = 0,
        },
        .scrollbar_size = 20,
        .auto_line_height = true,
        .selecting = false,
        .selection.active = false,
        .buffer = &buffer,
    };

    int arrow_press_interval = 70;

    SetTraceLogLevel(LOG_WARNING);

    const int fps = 60;
    const int ms_per_frame = 1000 / fps;
    bool mouse_button_left_was_pressed = false;
    bool arrow_left_was_pressed = false;
    bool arrow_right_was_pressed = false;
    int left_arrow_counter = 0;
    int right_arrow_counter = 0;
    SetTargetFPS(fps);
    while (!WindowShouldClose()) {
        
        TextDisplay_tick(&tdisp);

        if (IsWindowResized()) {
            tdisp.w = GetScreenWidth();
            tdisp.h = GetScreenHeight();
        }

        TextDisplay_onMouseMotion(&tdisp, GetMouseX(), GetMouseY());

        bool mouse_button_left_is_pressed = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (mouse_button_left_is_pressed && !mouse_button_left_was_pressed)
            TextDisplay_onClickDown(&tdisp, GetMouseX(), GetMouseY());
        else if (!mouse_button_left_is_pressed && mouse_button_left_was_pressed)
            TextDisplay_onClickUp(&tdisp, GetMouseX(), GetMouseY());
        mouse_button_left_was_pressed = mouse_button_left_is_pressed;

        Vector2 motion = GetMouseWheelMoveV();
        TextDisplay_onMouseWheel(&tdisp, 5 * motion.y);

        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            
            if (IsKeyPressed(KEY_C))
                TextDisplay_onCopy(&tdisp);
            
            if (IsKeyPressed(KEY_X))
                TextDisplay_onCut(&tdisp);
            
            if (IsKeyPressed(KEY_V))
                TextDisplay_onPaste(&tdisp);
            
            if (IsKeyPressed(KEY_S))
                save(file, &buffer);
        
        } else {

            bool arrow_left_is_pressed = IsKeyDown(KEY_LEFT);
            if (arrow_left_is_pressed) {
                if (!arrow_left_was_pressed) {
                    TextDisplay_onArrowLeftDown(&tdisp);
                    left_arrow_counter = 0;
                } else {
                    if (left_arrow_counter * ms_per_frame > arrow_press_interval) {
                        TextDisplay_onArrowLeftDown(&tdisp);
                        left_arrow_counter = 0;
                    } else
                        left_arrow_counter++;
                }
            }
            arrow_left_was_pressed = arrow_left_is_pressed;

            bool arrow_right_is_pressed = IsKeyDown(KEY_RIGHT);
            if (arrow_right_is_pressed) {
                if (!arrow_right_was_pressed) {
                    TextDisplay_onArrowRightDown(&tdisp);
                    right_arrow_counter = 0;
                } else {
                    if (right_arrow_counter * ms_per_frame > arrow_press_interval) {
                        TextDisplay_onArrowRightDown(&tdisp);
                        right_arrow_counter = 0;
                    } else
                        right_arrow_counter++;
                }
            }
            arrow_right_was_pressed = arrow_right_is_pressed;
            
            if (IsKeyPressed(KEY_ENTER))
                TextDisplay_onReturnDown(&tdisp);
            
            if (IsKeyPressed(KEY_BACKSPACE))
                TextDisplay_onBackspaceDown(&tdisp);

            int key = GetCharPressed();
            while (key > 0) {
                TextDisplay_onTextInput2(&tdisp, key);
                key = GetCharPressed();
            }
        }

        RenderTexture2D target = LoadRenderTexture(tdisp.w, tdisp.h);
        BeginTextureMode(target);
        TextDisplay_draw(&tdisp);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(RAYWHITE);
        Rectangle src, dst;
        src.x = 0;
        src.y = 0;
        src.width  = tdisp.w;
        src.height = -tdisp.h;
        dst.x = tdisp.x;
        dst.y = tdisp.y;
        dst.width  = tdisp.w;
        dst.height = tdisp.h;
        Vector2 org = {0, 0};
        DrawTexturePro(target.texture, src, 
                       dst, org, 0, WHITE);
        EndDrawing();
        UnloadRenderTexture(target);
    }
    GapBuffer_free(&buffer);

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