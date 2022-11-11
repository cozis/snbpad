#include <stdio.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "gap.h"
#include "gapiter.h"
#include "utils.h"
#include "textdisplay.h"

static void save(const char *file, 
                 GapBuffer *buffer)
{
    FILE *stream = fopen(file, "wb");
    if (stream == NULL) {
        fprintf(stderr, "WARNING :: Failed to open \"%s\" in write mode\n", file);
    } else {
        if (!GapBuffer_saveToStream(buffer, stream))
            fprintf(stderr, "WARNING :: Failed to save to \"%s\"\n", file);
        fclose(stream);
    }
}

static bool fileExists(const char *file)
{
    // Hacky but cross-platform!
    FILE *stream = fopen(file, "r"); 
    if (stream != NULL) {
        fclose(stream);
        return true;
    }
    return false;
}

static bool determineFileName(char *dst, size_t dstmax)
{
    int attempt = -1;
    do {
        attempt++;
        int k = snprintf(dst, dstmax, "unnamed-%d.txt", attempt);
        if (k < 0 || (size_t) k >= dstmax)
            return false;
    } while (fileExists(dst));
    return true;
}

void snbpad(const char *file, int w, int h)
{
    bool file_doesnt_exist_yet;

    char file_fallback[256];
    if (file == NULL) {
        if (!determineFileName(file_fallback, sizeof(file_fallback))) {
            fprintf(stderr, "ERROR :: Couldn't determine a valid file name\n");
            return;
        }
        file = file_fallback;
        file_doesnt_exist_yet = true;
    } else
        file_doesnt_exist_yet = !fileExists(file);

    if (w < 0) w = 300;
    if (h < 0) h = 400;

    if (SDL_Init(SDL_INIT_VIDEO))
        return;

    char caption[256];
    snprintf(caption, sizeof(caption), "SnBpad - %s", file);

    int flags = SDL_WINDOW_RESIZABLE;
    SDL_Window *win = SDL_CreateWindow(caption, SDL_WINDOWPOS_UNDEFINED, 
                                       SDL_WINDOWPOS_UNDEFINED, w, h, flags);
    if (win == NULL) {
        SDL_Quit();
        return;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
    if (ren == NULL) {
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }

    if (TTF_Init()) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }

    const char *font_file = "/usr/share/fonts/TTF/Inconsolata-Medium.ttf"; //"Monaco_5.1.ttf";
    size_t      font_size = 30;
    TTF_Font *font = TTF_OpenFont(font_file, font_size);
    if (font == NULL) {
        TTF_Quit();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }

    GapBuffer buffer;
    if (file_doesnt_exist_yet)
        GapBuffer_initEmpty(&buffer);
    else {
        if (!GapBuffer_initFile(&buffer, file)) {
            GapBuffer_initEmpty(&buffer);
            fprintf(stderr, "WARNING :: Failed to load \"%s\"\n", file);
        }
    }

    TextDisplay tdisp = {
        .x = 20,
        .y = 20,
        .w = w - 40,
        .h = h - 40,
        .lineno = {
            .hide = false,
            .nobg = false,
            .fgcolor = {0x00, 0x00, 0x00, 0xff},
            .bgcolor = {0xcc, 0xcc, 0xcc, 0xff},
            .font = font,
            .auto_width = false,
            .width = 70,
            .h_align = TextAlignH_CENTER,
            .v_align = TextAlignV_CENTER,
        },
        .text = {
            .nobg = true,
            .font = font,
            .selection_bgcolor = {0xcc, 0xcc, 0xcc, 0xff},
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

    bool lcontrol = false;
    bool rcontrol = false;
    bool quitting = false;
    while (!quitting) {
        
        TextDisplay_tick(&tdisp);

        SDL_Event event;
        while (!quitting && SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: quitting = true; break;
                
                case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    tdisp.w = event.window.data1 - 40;
                    tdisp.h = event.window.data2 - 40;
                    break;
                }
                break;

                case SDL_TEXTINPUT: TextDisplay_onTextInput(&tdisp, event.text.text, strlen(event.text.text)); break;
                case SDL_MOUSEWHEEL: TextDisplay_onMouseWheel(&tdisp, event.wheel.y); break;

                case SDL_KEYDOWN:
                {
                    SDL_Keycode code = event.key.keysym.sym;
                    if (lcontrol || rcontrol) {
                        switch (code) {
                            case SDLK_LCTRL: lcontrol = true; break;
                            case SDLK_RCTRL: rcontrol = true; break;
                            case SDLK_c: TextDisplay_onCopy(&tdisp); break;
                            case SDLK_x: TextDisplay_onCut(&tdisp); break;
                            case SDLK_v: TextDisplay_onPaste(&tdisp); break;
                            case SDLK_s: save(file, &buffer); break;
                        }
                    } else {
                        switch (code) {
                            case SDLK_LCTRL: lcontrol = true; break;
                            case SDLK_RCTRL: rcontrol = true; break;
                            case SDLK_LEFT:
                            case SDLK_RIGHT:     TextDisplay_onArrowDown(&tdisp, code); break;
                            case SDLK_TAB:       TextDisplay_onTabDown(&tdisp); break;
                            case SDLK_RETURN:    TextDisplay_onReturnDown(&tdisp); break;
                            case SDLK_BACKSPACE: TextDisplay_onBackspaceDown(&tdisp); break;
                        }
                    }
                }
                break;
                
                case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_LCTRL: lcontrol = false; break;
                    case SDLK_RCTRL: rcontrol = false; break;
                }
                break;

                case SDL_MOUSEMOTION: TextDisplay_onMouseMotion(&tdisp, event.motion.x, event.motion.y); break;
                case SDL_MOUSEBUTTONDOWN:
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: TextDisplay_onClickDown(&tdisp, event.button.x, event.button.y); break;
                }
                break;
                case SDL_MOUSEBUTTONUP:
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: TextDisplay_onClickUp(&tdisp, event.button.x, event.button.y); break;
                }
                break;
            }
        }
        SDL_SetRenderDrawColor(ren, 
            0xff, 0xff, 0xff, 0xff);
        SDL_RenderClear(ren);
        TextDisplay_draw(&tdisp, ren);
        SDL_RenderPresent(ren);
        SDL_Delay(20); 
    }
    GapBuffer_free(&buffer);

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
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