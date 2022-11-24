#include "guielement.h"

typedef struct {
    Color bgcolor;
    Color fgcolor;
    const unsigned char *font_data;
    size_t               font_data_size;
    const char  *font_file;
    unsigned int font_size;
    bool auto_line_height;
    size_t    line_height;
    size_t padding_top;
    size_t padding_left;
    size_t subtree_padding_left;
} TreeViewStyle;

GUIElement *TreeView_new(Rectangle region,
                         const char *name,
                         const char *full_path,
                         void (*callback)(const char*, size_t, void*),
                         const TreeViewStyle *style,
                         void *userp);