#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "splitview.h"

typedef struct {
    GUIElement base;
    Rectangle old_region;
    SplitDirection dir;
    GUIElement *children[2];
    struct {
        bool active;
        int start_x_or_y_relative_to_separator;
    } reshaping;
    const SplitViewStyle *style;
} SplitView;

static void freeCallback(GUIElement *elem)
{
    SplitView *sv = (SplitView*) elem;
    GUIElement_free(sv->children[0]);
    GUIElement_free(sv->children[1]);
    free(elem);
}

static void tickCallback(GUIElement *elem, uint64_t time_in_ms)
{
    SplitView *sv = (SplitView*) elem;

    bool  width_changed = (sv->old_region.width != sv->base.region.width);
    bool height_changed = (sv->old_region.height != sv->base.region.height);
    if (width_changed) {
        if (sv->dir == SplitDirection_HORIZONTAL) {
            int separator = sv->style->separator.size;
            switch (sv->style->resize_mode) {
                case SplitResizeMode_KEEPRATIOS: {
                    int old_available = sv->old_region.width - separator;
                    int new_available = elem->region.width   - separator;

                    int old_left_width = sv->children[0]->region.width;
                    double left_ratio = (double) old_left_width / old_available;
                    int new_left_width = left_ratio * new_available;

                    sv->children[0]->region.width = new_left_width;
                    sv->children[1]->region.width = new_available - new_left_width;
                    sv->children[1]->region.x = sv->children[0]->region.x + new_left_width + separator;
                } break;

                case SplitResizeMode_RESIZELEFT: {
                    int new_available = elem->region.width - separator;
                    int new_left_width = new_available - sv->children[1]->region.width;
                    sv->children[0]->region.width = new_left_width;
                    
                    int left_x = sv->children[0]->region.x;
                    sv->children[1]->region.x = left_x + new_left_width + separator;
                } break;

                case SplitResizeMode_RESIZERIGHT: {
                    int new_available = elem->region.width - separator;
                    sv->children[1]->region.width = new_available - sv->children[0]->region.width;
                } break;
            }
        } else {
            sv->children[0]->region.width = elem->region.width;
            sv->children[1]->region.width = elem->region.width;
        }
    }
    if (height_changed) {
        if (sv->dir == SplitDirection_VERTICAL) {
            int separator = sv->style->separator.size;
            switch (sv->style->resize_mode) {
                case SplitResizeMode_KEEPRATIOS: {
                    int old_available = sv->old_region.height - separator;
                    int new_available = elem->region.height   - separator;

                    int old_left_height = sv->children[0]->region.height;
                    double left_ratio = (double) old_left_height / old_available;
                    int new_left_height = left_ratio * new_available;

                    sv->children[0]->region.height = new_left_height;
                    sv->children[1]->region.height = new_available - new_left_height;
                    sv->children[1]->region.y = sv->children[0]->region.y + new_left_height + separator;
                } break;

                case SplitResizeMode_RESIZELEFT: {
                    int new_available = elem->region.height - separator;
                    int new_left_height = new_available - sv->children[1]->region.height;
                    sv->children[0]->region.height = new_left_height;
                    
                    int left_y = sv->children[0]->region.y;
                    sv->children[1]->region.y = left_y + new_left_height + separator;
                } break;

                case SplitResizeMode_RESIZERIGHT: {
                    int new_available = elem->region.height - separator;
                    sv->children[1]->region.height = new_available - sv->children[0]->region.height;
                } break;
            }
        } else {
            sv->children[0]->region.height = elem->region.height;
            sv->children[1]->region.height = elem->region.height;
        }
    }
    sv->old_region = sv->base.region;

    GUIElement_tick(sv->children[0], time_in_ms);
    GUIElement_tick(sv->children[1], time_in_ms);
}

static void onMouseMotionCallback(GUIElement *elem, 
                                  int x, int y)
{
    int abs_x = x + elem->region.x;
    int abs_y = y + elem->region.y;

    SplitView *sv = (SplitView*) elem;
    if (sv->reshaping.active) {
        sv->reshaping.active = true;
        switch (sv->dir) {
            case SplitDirection_VERTICAL: {
                int sep_y = abs_y - sv->reshaping.start_x_or_y_relative_to_separator;
                sv->children[0]->region.height = sep_y - sv->children[0]->region.y;
                sv->children[1]->region.y = sep_y + sv->style->separator.size;
                sv->children[1]->region.height = elem->region.height - sv->style->separator.size - sv->children[0]->region.height;
            } break;
            
            case SplitDirection_HORIZONTAL: {
                int sep_x = abs_x - sv->reshaping.start_x_or_y_relative_to_separator;
                sv->children[0]->region.width = sep_x - sv->children[0]->region.x;
                sv->children[1]->region.x = sep_x + sv->style->separator.size;
                sv->children[1]->region.width = elem->region.width - sv->style->separator.size - sv->children[0]->region.width;
            } break;
        }
    }
    GUIElement_onMouseMotion(sv->children[0], 
        abs_x - sv->children[0]->region.x, 
        abs_y - sv->children[0]->region.y);
    GUIElement_onMouseMotion(sv->children[1], 
        abs_x - sv->children[1]->region.x, 
        abs_y - sv->children[1]->region.y);
}

static void clickUpCallback(GUIElement *elem, 
                            int x, int y)
{
    SplitView *sv = (SplitView*) elem;
    sv->reshaping.active = false;

    int abs_x = x + elem->region.x;
    int abs_y = y + elem->region.y;
    GUIElement_clickUp(sv->children[0],
        abs_x - sv->children[0]->region.x, 
        abs_y - sv->children[0]->region.y);
    GUIElement_clickUp(sv->children[1],
        abs_x - sv->children[1]->region.x, 
        abs_y - sv->children[1]->region.y);
}

static Rectangle
getSeparatorRegion(SplitView *sv)
{
    Rectangle region;
    switch (sv->dir) {
        case SplitDirection_VERTICAL:
        region.x = sv->base.region.x;
        region.y = sv->base.region.y + sv->children[0]->region.height;
        region.width = sv->base.region.width;
        region.height = sv->style->separator.size;
        break;
        case SplitDirection_HORIZONTAL:
        region.x = sv->base.region.x + sv->children[0]->region.width;
        region.y = sv->base.region.y;
        region.width = sv->style->separator.size;
        region.height = sv->base.region.height;
        break;
    }
    return region;
}

static GUIElement*
onClickDownCallback(GUIElement *elem, 
                    int x, int y)
{
    SplitView *sv = (SplitView*) elem;

    // Note: We're assuming the children's
    //       regions are updated.
    int abs_x = elem->region.x + x;
    int abs_y = elem->region.y + y;
    Vector2 point = { abs_x, abs_y };
    if (CheckCollisionPointRec(point, sv->children[0]->region))
        return GUIElement_onClickDown(
            sv->children[0], 
            abs_x - sv->children[0]->region.x,
            abs_y - sv->children[0]->region.y
        );
    if (CheckCollisionPointRec(point, sv->children[1]->region))
        return GUIElement_onClickDown(
            sv->children[1], 
            abs_x - sv->children[1]->region.x,
            abs_y - sv->children[1]->region.y
        );

    Rectangle separator = getSeparatorRegion(sv);
    assert(CheckCollisionPointRec(point, separator));

    {
        sv->reshaping.active = true;
        switch (sv->dir) {
            case SplitDirection_VERTICAL:   sv->reshaping.start_x_or_y_relative_to_separator = abs_y - separator.y; break;
            case SplitDirection_HORIZONTAL: sv->reshaping.start_x_or_y_relative_to_separator = abs_x - separator.x; break;
        }
    }
    return NULL;
}

static void offClickDownCallback(GUIElement *elem)
{
    SplitView *sv = (SplitView*) elem;
    GUIElement_offClickDown(sv->children[0]);
    GUIElement_offClickDown(sv->children[1]);
}

static GUIElement*
getHoveredCallback(GUIElement *elem, 
                   int x, int y)
{
    int abs_x = x + elem->region.x;
    int abs_y = y + elem->region.y;
    Vector2 point = {abs_x, abs_y};
    SplitView *sv = (SplitView*) elem;
    if (CheckCollisionPointRec(point, sv->children[0]->region))
        return sv->children[0];
    if (CheckCollisionPointRec(point, sv->children[1]->region))
        return sv->children[1];
    return elem;
}

static void drawCallback(GUIElement *elem)
{
    SplitView *sv = (SplitView*) elem;
    GUIElement_draw(sv->children[0]);
    GUIElement_draw(sv->children[1]);
}

static const GUIElementMethods methods = {
    .free = freeCallback,
    .tick = tickCallback,
    .draw = drawCallback,
    .clickUp = clickUpCallback,
    .onFocusLost = NULL,
    .onFocusGained = NULL,
    .onClickDown = onClickDownCallback,
    .offClickDown = offClickDownCallback,
    .onMouseWheel = NULL,
    .onMouseMotion = onMouseMotionCallback,
    .onArrowLeftDown = NULL,
    .onArrowRightDown = NULL,
    .onReturnDown = NULL,
    .onBackspaceDown = NULL,
    .onTabDown = NULL,
    .onTextInput = NULL,
    .onPaste = NULL,
    .onCopy = NULL,
    .onCut = NULL,
    .onSave = NULL,
    .onOpen = NULL,
    .getHovered = getHoveredCallback,
};

GUIElement *SplitView_new(Rectangle region,
                          const char *name,
                          GUIElement *child_0,
                          GUIElement *child_1,
                          SplitDirection dir,
                          const SplitViewStyle *style)
{
    SplitView *sv = malloc(sizeof(SplitView));
    if (sv != NULL) {
        sv->base.region = region;
        sv->base.methods = &methods;
        strncpy(sv->base.name, name, 
                sizeof(sv->base.name));
        sv->base.name[sizeof(sv->base.name)-1] = '\0';
        sv->children[0] = child_0;
        sv->children[1] = child_1;
        sv->old_region = region;
        sv->dir = dir;
        sv->style = style;
        sv->reshaping.active = false;

        switch (dir) {
            case SplitDirection_VERTICAL: {
                int sep_w = style->separator.size;
                int view_h = (region.height - sep_w) / 2;
                child_0->region.x = region.x;
                child_0->region.y = region.y;
                child_0->region.width  = region.width;
                child_0->region.height = view_h;
                child_1->region.x = region.x;
                child_1->region.y = region.y + view_h + sep_w;
                child_1->region.width  = region.width;
                child_1->region.height = view_h;
            } break;
            case SplitDirection_HORIZONTAL: {
                int sep_w = style->separator.size;
                int view_w = (region.width - sep_w) / 2;
                child_0->region.x = region.x;
                child_0->region.y = region.y;
                child_0->region.width  = view_w;
                child_0->region.height = region.height;
                child_1->region.x = region.x + view_w + sep_w;
                child_1->region.y = region.y;
                child_1->region.width  = view_w;
                child_1->region.height = region.height;
            } break;
        }
    }
    return (GUIElement*) sv;
}