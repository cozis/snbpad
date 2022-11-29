#ifndef SCROLLBAR_H
#define SCROLLBAR_H
#include "guielement.h"

typedef enum {
    ScrollbarDirection_VERTICAL,
    ScrollbarDirection_HORIZONTAL,
} ScrollbarDirection;

typedef struct {
    int inertia;
    int size;
} ScrollbarStyle;

typedef struct {
    ScrollbarDirection direction;
    const ScrollbarStyle *style;
    GUIElement *parent;
    int  force;
    int  amount;
    bool active;
    int start_amount;
    int start_cursor;
} Scrollbar;

void Scrollbar_init(Scrollbar *state, ScrollbarDirection dir, GUIElement *parent, const ScrollbarStyle *style);
void Scrollbar_free(Scrollbar *state);
void scrollbar_draw(Scrollbar *scrollbar);
int  Scrollbar_getValue(Scrollbar *state);
void Scrollbar_setValue(Scrollbar *state, int value);
void Scrollbar_addValue(Scrollbar *state, int delta);
void Scrollbar_addForce(Scrollbar *state, int delta);
void Scrollbar_tick(Scrollbar *state, uint64_t time_in_ms);
bool Scrollbar_onMouseMotion(Scrollbar *state, int u);
bool Scrollbar_onClickDown(Scrollbar *scrollbar, int x, int y);
void Scrollbar_clickUp(Scrollbar *scrollbar);
void Scrollbar_getRegion(Scrollbar *state, Rectangle *scrollbar, Rectangle *thumb);
#endif