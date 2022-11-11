#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef enum {
    TextAlignH_LEFT,
    TextAlignH_RIGHT,
    TextAlignH_CENTER,
} TextAlignH;

typedef enum {
    TextAlignV_TOP,
    TextAlignV_BOTTOM,
    TextAlignV_CENTER,
} TextAlignV;

typedef struct {
    bool active;
    size_t start, end;
} Selection;

typedef struct {
    int x, y;
    int w, h;
    struct {
        bool hide;
        bool nobg;
        SDL_Color fgcolor;
        SDL_Color bgcolor;
        TTF_Font *font;
        bool    auto_width;
        unsigned int width;
        TextAlignH h_align;
        TextAlignV v_align;
    } lineno;
    struct {
        bool nobg;
        SDL_Color fgcolor;
        SDL_Color bgcolor;
        TTF_Font *font;
        SDL_Color selection_bgcolor;
    } text;
    struct {
        SDL_Color bgcolor;
    } cursor;
    unsigned int scrollbar_size;
    bool    auto_line_height;
    unsigned int line_height;
    struct {
        int  inertia;
        int  force;
        int  amount;
        bool active;
        int start_amount;
        int start_cursor;
    } v_scroll;
    bool      selecting;
    Selection selection;
    GapBuffer *buffer;
} TextDisplay;

void TextDisplay_tick(TextDisplay *tdisp);
void TextDisplay_draw(TextDisplay *tdisp, SDL_Renderer *ren);
void TextDisplay_onClickUp(TextDisplay *tdisp, int x, int y);
void TextDisplay_onClickDown(TextDisplay *tdisp, int x, int y);
void TextDisplay_onMouseWheel(TextDisplay *tdisp, int y);
void TextDisplay_onMouseMotion(TextDisplay *tdisp, int x, int y);
void TextDisplay_onArrowDown(TextDisplay *tdisp, SDL_Keycode code);
void TextDisplay_onBackspaceDown(TextDisplay *tdisp);
void TextDisplay_onReturnDown(TextDisplay *tdisp);
void TextDisplay_onTabDown(TextDisplay *tdisp);
void TextDisplay_onTextInput(TextDisplay *tdisp, const char *str, size_t len);
void TextDisplay_onPaste(TextDisplay *tdisp);
void TextDisplay_onCopy(TextDisplay *tdisp);
void TextDisplay_onCut(TextDisplay *tdisp);