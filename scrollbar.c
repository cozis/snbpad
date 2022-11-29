#include <assert.h>
#include <string.h>
#include "scrollbar.h"

void Scrollbar_init(Scrollbar *state, ScrollbarDirection dir, 
                    GUIElement *parent, const ScrollbarStyle *style)
{
    state->direction = dir;
    state->parent = parent;
    state->style = style;
    state->force = 0;
    state->amount = 0;
    state->active = false;
    state->start_amount = 0;
    state->start_cursor = 0;
}

void Scrollbar_free(Scrollbar *state)
{
    (void) state;
}

int Scrollbar_getValue(Scrollbar *state)
{
    return state->amount;
}

static int Scrollbar_getMaximumValue(Scrollbar *state)
{
    int logical_width;
    int logical_height;
    GUIElement_getLogicalSize(state->parent, 
                              &logical_width, 
                              &logical_height);

    Rectangle region = GUIElement_getRegion(state->parent);
    int real_width  = region.width;
    int real_height = region.height;

    int max_scroll;
    switch (state->direction) {
        
        case ScrollbarDirection_VERTICAL:
        if (logical_height < real_height)
            max_scroll = 0;
        else
            max_scroll = (logical_height - real_height);
        break;
        
        case ScrollbarDirection_HORIZONTAL:
        if (logical_width < real_width)
            max_scroll = 0;
        else
            max_scroll = (logical_width - real_width);
        break;
    }

    return max_scroll;
}

static bool Scrollbar_reachedLowerLimit(Scrollbar *state)
{
    return Scrollbar_getValue(state) == 0;
}

static bool Scrollbar_reachedUpperLimit(Scrollbar *state)
{
    return Scrollbar_getValue(state) == Scrollbar_getMaximumValue(state);
}

void Scrollbar_setValue(Scrollbar *state, int value)
{
    int logical_width;
    int logical_height;
    GUIElement_getLogicalSize(state->parent, 
                              &logical_width, 
                              &logical_height);

    int max_scroll = Scrollbar_getMaximumValue(state);

    int adjusted_value = value;

    if (adjusted_value < 0)
        adjusted_value = 0;

    if (adjusted_value > max_scroll)
        adjusted_value = max_scroll;

    state->amount = adjusted_value;
}

void Scrollbar_addValue(Scrollbar *state, int delta)
{
    int value = delta + Scrollbar_getValue(state);
    Scrollbar_setValue(state, value);
}

void Scrollbar_addForce(Scrollbar *state, int delta)
{
    state->force += delta;
}

void Scrollbar_tick(Scrollbar *state, uint64_t time_in_ms)
{
    int inertia = state->style->inertia;
    int   force = state->force;
    
    if (force != 0) {
        Scrollbar_addValue(state, force);
        if (force < 0) {
            force += inertia;
            if (force > 0) force = 0;
            if (Scrollbar_reachedLowerLimit(state))
                force = 0;
        } else {
            force -= inertia;
            if (force < 0) force = 0;
            if (Scrollbar_reachedUpperLimit(state))
                force = 0;
        }
    }
    state->force = force;
}

static int getParentLogicSize(Scrollbar *state)
{
    int logic_width;
    int logic_height;
    GUIElement_getLogicalSize(state->parent, 
                              &logic_width, 
                              &logic_height);

    int logic_size;
    
    switch (state->direction) {
        case ScrollbarDirection_VERTICAL:
        logic_size = logic_height;
        break;
        case ScrollbarDirection_HORIZONTAL:
        logic_size = logic_width;
        break;
    }

    return logic_size;
}

static int getParentRealSize(Scrollbar *state, int *other)
{
    Rectangle region = GUIElement_getRegion(state->parent);

    int real_size;
    int other_real_size;
    switch (state->direction) {
        case ScrollbarDirection_VERTICAL:
        real_size = region.height;
        other_real_size = region.width;
        break;
        case ScrollbarDirection_HORIZONTAL:
        real_size = region.width;
        other_real_size = region.height;
        break;
    }
    if (other != NULL)
        *other = other_real_size;
    return real_size;
}

bool Scrollbar_onMouseMotion(Scrollbar *state, int u)
{
    if (state->active) {
        
        int logic_size = getParentLogicSize(state);
        int  real_size = getParentRealSize(state, NULL);
        double ratio = (double) logic_size / real_size;
        
        int delta = u - state->start_cursor;
        int amount = state->start_amount + delta * ratio;
        Scrollbar_setValue(state, amount);
    }
    return state->active;
}

bool Scrollbar_onClickDown(Scrollbar *scrollbar, int x, int y)
{
    Rectangle unused, thumb;
    Scrollbar_getRegion(scrollbar, &unused, &thumb);
    if (CheckCollisionPointRec((Vector2) {x, y}, thumb)) {

        int u;
        switch (scrollbar->direction) {
            case ScrollbarDirection_VERTICAL:   u = y; break;
            case ScrollbarDirection_HORIZONTAL: u = x; break;
        }

        scrollbar->active = true;
        scrollbar->start_cursor = u;
        scrollbar->start_amount = scrollbar->amount;
        return true;
    } else
        return false;
}

void Scrollbar_clickUp(Scrollbar *scrollbar)
{
    scrollbar->active = false;
}

void
Scrollbar_getRegion(Scrollbar *state,
                    Rectangle *scrollbar, 
                    Rectangle *thumb)
{
    int other_real_size;
    int real_size = getParentRealSize(state, &other_real_size);
    int logic_size = getParentLogicSize(state);
    
    int thumb_u    = real_size * (double) state->amount / logic_size;
    int thumb_size = real_size * (double)     real_size / logic_size;
    
    if (logic_size <= real_size) {
        memset(scrollbar, 0, sizeof(Rectangle));
        memset(thumb,     0, sizeof(Rectangle));
    } else if (state->direction == ScrollbarDirection_VERTICAL) {
        scrollbar->width  = state->style->size;
        scrollbar->height = real_size;
        scrollbar->x = other_real_size - scrollbar->width;
        scrollbar->y = 0;
        thumb->x = scrollbar->x;
        thumb->y = scrollbar->y + thumb_u;
        thumb->width  = scrollbar->width;
        thumb->height = thumb_size;
    } else {
        assert(state->direction == ScrollbarDirection_HORIZONTAL);
        scrollbar->height = state->style->size;
        scrollbar->width  = real_size;
        scrollbar->y = other_real_size - scrollbar->height;
        scrollbar->x = 0;
        thumb->y = scrollbar->y;
        thumb->x = scrollbar->x + thumb_u;
        thumb->height = scrollbar->height;
        thumb->width  = thumb_size;
    }
}

void scrollbar_draw(Scrollbar *scrollbar)
{
    Rectangle unused, thumb;
    Scrollbar_getRegion(scrollbar, &unused, &thumb);
    DrawRectangle(thumb.x, thumb.y,
                  thumb.width, 
                  thumb.height,
                  RED);
}