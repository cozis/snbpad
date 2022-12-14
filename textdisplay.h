#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>
#include "guielement.h"
#include "scrollbar.h"

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
    struct {
        bool hide;
        bool nobg;
        Color fgcolor;
        Color bgcolor;
        const unsigned char *font_data;
        size_t               font_data_size;
        const char *font_file;
        int         font_size;
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
        const unsigned char *font_data;
        size_t               font_data_size;
        const char *font_file;
        int         font_size;
        TextAlignV v_align;
        Color selection_bgcolor;
    } text;
    struct {
        Color bgcolor;
    } cursor;
    ScrollbarStyle *v_scroll;
    ScrollbarStyle *h_scroll;
    bool    auto_line_height;
    unsigned int line_height;
} TextDisplayStyle;

GUIElement *TextDisplay_new(Rectangle region, const char *name, const char *file, const TextDisplayStyle *style);
