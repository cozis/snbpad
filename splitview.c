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

static void onResizeCallback(GUIElement *elem, 
                             Rectangle old_region)
{
    SplitView *sv = (SplitView*) elem;

    Rectangle new_region = elem->region;
    Rectangle old_subregion_1 = GUIElement_getRegion(sv->children[0]);
    Rectangle old_subregion_2 = GUIElement_getRegion(sv->children[1]);
    Rectangle new_subregion_1;
    Rectangle new_subregion_2;

    const int sep = sv->style->separator.size;

    if (sv->dir == SplitDirection_HORIZONTAL) {
        int old_available = old_region.width - sep;
        int new_available = new_region.width - sep;
        new_subregion_1.x = new_region.x;
        new_subregion_1.y = new_region.y;
        new_subregion_1.height = new_region.height;
        new_subregion_2.y = new_region.y;
        new_subregion_2.height = new_region.height;
        switch (sv->style->resize_mode) {
            case SplitResizeMode_KEEPRATIOS:
            new_subregion_1.width = new_available * (double) old_subregion_1.width / old_available;
            new_subregion_2.width = new_available - new_subregion_1.width;
            break;
            case SplitResizeMode_RESIZELEFT:
            new_subregion_1.width = new_available - old_subregion_2.width;
            new_subregion_2.width = old_subregion_2.width;
            break;
            case SplitResizeMode_RESIZERIGHT:
            new_subregion_1.width = old_subregion_1.width;
            new_subregion_2.width = new_available - old_subregion_1.width;
            break;
        }
        new_subregion_2.x = new_subregion_1.x + new_subregion_1.width + sep;
    } else {
        int old_available = old_region.height - sep;
        int new_available = new_region.height - sep;
        new_subregion_1.y = new_region.y;
        new_subregion_1.x = new_region.x;
        new_subregion_1.width = new_region.width;
        new_subregion_2.x = new_region.x;
        new_subregion_2.width = new_region.width;
        switch (sv->style->resize_mode) {
            case SplitResizeMode_KEEPRATIOS:
            new_subregion_1.height = new_available * (double) old_subregion_1.height / old_available;
            new_subregion_2.height = new_available - new_subregion_1.height;
            break;
            case SplitResizeMode_RESIZELEFT:
            new_subregion_1.height = new_available - old_subregion_2.height;
            new_subregion_2.height = old_subregion_2.height;
            break;
            case SplitResizeMode_RESIZERIGHT:
            new_subregion_1.height = old_subregion_1.height;
            new_subregion_2.height = new_available - old_subregion_1.height;
            break;
        }
        new_subregion_2.y = new_subregion_1.y + new_subregion_1.height + sep;
    }
    GUIElement_setRegion(sv->children[0], new_subregion_1);
    GUIElement_setRegion(sv->children[1], new_subregion_2);
}

static void tickCallback(GUIElement *elem, uint64_t time_in_ms)
{
    SplitView *sv = (SplitView*) elem;
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
        
        Rectangle region = GUIElement_getRegion(elem);
        
        Rectangle new_subregion_1;
        Rectangle new_subregion_2;

        int sep = sv->style->separator.size;
        new_subregion_1.x = region.x;
        new_subregion_1.y = region.y;
        switch (sv->dir) {

            case SplitDirection_VERTICAL: {
                int sep_y = abs_y - sv->reshaping.start_x_or_y_relative_to_separator;
                new_subregion_1.width  = region.width;
                new_subregion_1.height = sep_y - region.y;
                new_subregion_2.x = region.x;
                new_subregion_2.y = sep_y + sep;
                new_subregion_2.width  = region.width;
                new_subregion_2.height = region.height - new_subregion_1.height - sep;
            } break;
            
            case SplitDirection_HORIZONTAL: {
                int sep_x = abs_x - sv->reshaping.start_x_or_y_relative_to_separator;
                new_subregion_1.width = sep_x - region.x;
                new_subregion_1.height  = region.height;
                new_subregion_2.x = sep_x + sep;
                new_subregion_2.y = region.y;
                new_subregion_2.height = region.height;
                new_subregion_2.width  = region.width - new_subregion_1.width - sep;
            } break;
        }
        GUIElement_setRegion(sv->children[0], new_subregion_1);
        GUIElement_setRegion(sv->children[1], new_subregion_2);
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

    Rectangle subregion_1 = GUIElement_getRegion(sv->children[0]);
    Rectangle subregion_2 = GUIElement_getRegion(sv->children[1]);

    if (CheckCollisionPointRec(point, subregion_1))
        return GUIElement_getHovered(sv->children[0],
                                     abs_x - subregion_1.x,
                                     abs_y - subregion_1.y);

    if (CheckCollisionPointRec(point, subregion_2))
        return GUIElement_getHovered(sv->children[1],
                                     abs_x - subregion_2.x,
                                     abs_y - subregion_2.y);
    return elem;
}

static void drawCallback(GUIElement *elem)
{
    SplitView *sv = (SplitView*) elem;
    Rectangle separator = getSeparatorRegion(sv);
    DrawRectangle(separator.x, 
                  separator.y,
                  separator.width,
                  separator.height,
                  sv->style->bgcolor);
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
    .onResize = onResizeCallback,
    .openFile = NULL,
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

        int sep = style->separator.size;
        Rectangle subregion_1;
        Rectangle subregion_2;
        switch (dir) {
            case SplitDirection_VERTICAL: {
                int available = (region.height - sep) / 2;
                subregion_1.x = region.x;
                subregion_1.y = region.y;
                subregion_1.width  = region.width;
                subregion_1.height = available;
                subregion_2.x = region.x;
                subregion_2.y = region.y + available + sep;
                subregion_2.width  = region.width;
                subregion_2.height = available;
            } break;
            case SplitDirection_HORIZONTAL: {
                int available = (region.width - sep) / 2;
                subregion_1.x = region.x;
                subregion_1.y = region.y;
                subregion_1.width  = available;
                subregion_1.height = region.height;
                subregion_2.x = region.x + available + sep;
                subregion_2.y = region.y;
                subregion_2.width  = available;
                subregion_2.height = region.height;
            } break;
        }
        GUIElement_setRegion(child_0, subregion_1);
        GUIElement_setRegion(child_1, subregion_2);
    }
    return (GUIElement*) sv;
}