#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>
#include "guielement.h"

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
        const char *font_file;
        int         font_size;
        TextAlignV v_align;
        Color selection_bgcolor;
    } text;
    struct {
        Color bgcolor;
    } cursor;
    struct {
        int inertia;
    } scroll;
    unsigned int scrollbar_size;
    bool    auto_line_height;
    unsigned int line_height;
} TextDisplayStyle;

GUIElement *TextDisplay_new(Rectangle region, const char *name, const char *file, const TextDisplayStyle *style);
