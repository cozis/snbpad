#ifndef GUIELEMENT_H
#define GUIELEMENT_H

#include <stdint.h>
#include <stddef.h>
#include <raylib.h>

typedef struct GUIElement GUIElement;

typedef struct {
    void (*free)(GUIElement*);
    void (*tick)(GUIElement*, uint64_t);
    void (*draw)(GUIElement*);
    void (*clickUp)(GUIElement*, int, int);
    GUIElement *(*onClickDown)(GUIElement*, int, int);
    GUIElement *(*getHovered)(GUIElement*, int, int);
    void (*offClickDown)(GUIElement*);
    void (*onMouseWheel)(GUIElement*, int);
    void (*onMouseMotion)(GUIElement*, int, int);
    void (*onArrowLeftDown)(GUIElement*);
    void (*onArrowRightDown)(GUIElement*);
    void (*onReturnDown)(GUIElement*);
    void (*onBackspaceDown)(GUIElement*);
    void (*onTextInput)(GUIElement*, const char*, size_t);
    void (*onPaste)(GUIElement*);
    void (*onCopy)(GUIElement*);
    void (*onCut)(GUIElement*);
    void (*onSave)(GUIElement*);
    void (*onOpen)(GUIElement*);
    void (*onFocusLost)(GUIElement*);
    void (*onFocusGained)(GUIElement*);
    void (*onResize)(GUIElement*, Rectangle);
    bool (*openFile)(GUIElement*, const char*);
    void (*getMinimumSize)(GUIElement*, int*, int*);
    void (*getLogicalSize)(GUIElement*, int*, int*);
} GUIElementMethods;

struct GUIElement {
    const GUIElementMethods *methods;
    Rectangle region;
    char name[16];
};

void GUIElement_free(GUIElement *elem);
void GUIElement_tick(GUIElement *elem, uint64_t time);
void GUIElement_draw(GUIElement *elem);
void GUIElement_clickUp(GUIElement *elem, int x, int y);
GUIElement *GUIElement_onClickDown(GUIElement *elem, int x, int y);
void GUIElement_offClickDown(GUIElement *elem);
void GUIElement_onMouseWheel(GUIElement *elem, int y);
void GUIElement_onMouseMotion(GUIElement *elem, int x, int y);
void GUIElement_onArrowLeftDown(GUIElement *elem);
void GUIElement_onArrowRightDown(GUIElement *elem);
void GUIElement_onReturnDown(GUIElement *elem);
void GUIElement_onBackspaceDown(GUIElement *elem);
void GUIElement_onTabDown(GUIElement *elem);
void GUIElement_onTextInput(GUIElement *elem, const char *str, size_t len);
void GUIElement_onTextInput2(GUIElement *elem, uint32_t rune);
void GUIElement_onPaste(GUIElement *elem);
void GUIElement_onCopy(GUIElement *elem);
void GUIElement_onCut(GUIElement *elem);
void GUIElement_onSave(GUIElement *elem);
void GUIElement_onOpen(GUIElement *elem);
void GUIElement_onFocusLost(GUIElement *elem);
void GUIElement_onFocusGained(GUIElement *elem);
GUIElement *GUIElement_getHovered(GUIElement *elem, int x, int y);
void      GUIElement_setRegion(GUIElement *elem, Rectangle region);
Rectangle GUIElement_getRegion(GUIElement *elem);
bool GUIElement_openFile(GUIElement *elem, const char *file);
void GUIElement_getMinimumSize(GUIElement *elem, int *w, int *h);
void GUIElement_getLogicalSize(GUIElement *elem, int *w, int *h);
#endif