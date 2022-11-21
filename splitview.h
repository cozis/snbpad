//#include <stdint.h>
//#include <stdbool.h>
#include <raylib.h>
#include "guielement.h"

typedef enum {
    SplitResizeMode_KEEPRATIOS,
    SplitResizeMode_RESIZELEFT,
    SplitResizeMode_RESIZERIGHT,
} SplitResizeMode;

typedef struct {
    struct {
        unsigned int size;
    } separator;
    SplitResizeMode resize_mode;
} SplitViewStyle;

typedef enum {
    SplitDirection_VERTICAL,
    SplitDirection_HORIZONTAL,
} SplitDirection;

GUIElement *SplitView_new(Rectangle region,
                          const char *name,
                          GUIElement *child_0,
                          GUIElement *child_1,
                          SplitDirection dir,
                          const SplitViewStyle *style);
