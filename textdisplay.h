#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>

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
        Color fgcolor;
        Color bgcolor;
        Font *font;
        bool    auto_width;
        unsigned int width;
        unsigned char padding_up;
        unsigned char padding_down;
        unsigned char padding_left;
        unsigned char padding_right;
        TextAlignH h_align;
        TextAlignV v_align;
    } lineno;
    struct {
        bool nobg;
        Color fgcolor;
        Color bgcolor;
        Font *font;
        Color selection_bgcolor;
    } text;
    struct {
        Color bgcolor;
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
void TextDisplay_draw(TextDisplay *tdisp);
void TextDisplay_onClickUp(TextDisplay *tdisp, int x, int y);
void TextDisplay_onClickDown(TextDisplay *tdisp, int x, int y);
void TextDisplay_onMouseWheel(TextDisplay *tdisp, int y);
void TextDisplay_onMouseMotion(TextDisplay *tdisp, int x, int y);
void TextDisplay_onArrowLeftDown(TextDisplay *tdisp);
void TextDisplay_onArrowRightDown(TextDisplay *tdisp);
void TextDisplay_onBackspaceDown(TextDisplay *tdisp);
void TextDisplay_onReturnDown(TextDisplay *tdisp);
void TextDisplay_onTabDown(TextDisplay *tdisp);
void TextDisplay_onTextInput(TextDisplay *tdisp, const char *str, size_t len);
void TextDisplay_onTextInput2(TextDisplay *tdisp, uint32_t rune);
void TextDisplay_onPaste(TextDisplay *tdisp);
void TextDisplay_onCopy(TextDisplay *tdisp);
void TextDisplay_onCut(TextDisplay *tdisp);