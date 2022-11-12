#include <stdint.h>
#include <stddef.h>
#include <raylib.h>

typedef struct GUIElement GUIElement;

typedef struct {
    bool allows_focus;
    void (*free)(GUIElement*);
    void (*tick)(GUIElement*);
    void (*draw)(GUIElement*);
    void (*clickUp)(GUIElement*, int, int);
    void (*onClickDown)(GUIElement*, int, int);
    void (*offClickDown)(GUIElement*);
    void (*onMouseWheel)(GUIElement*, int);
    void (*onMouseMotion)(GUIElement*, int, int);
    void (*onArrowLeftDown)(GUIElement*);
    void (*onArrowRightDown)(GUIElement*);
    void (*onReturnDown)(GUIElement*);
    void (*onBackspaceDown)(GUIElement*);
    void (*onTabDown)(GUIElement*);
    void (*onTextInput)(GUIElement*, const char*, size_t);
    void (*onPaste)(GUIElement*);
    void (*onCopy)(GUIElement*);
    void (*onCut)(GUIElement*);
    void (*onSave)(GUIElement*);
    void (*onOpen)(GUIElement*);
} GUIElementMethods;

struct GUIElement {
    const GUIElementMethods *methods;
    Rectangle region;
    char name[16];
};

void GUIElement_free(GUIElement *elem);
void GUIElement_tick(GUIElement *elem);
void GUIElement_draw(GUIElement *elem);
void GUIElement_clickUp(GUIElement *elem, int x, int y);
void GUIElement_onClickDown(GUIElement *elem, int x, int y);
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
bool GUIElement_allowsFocus(GUIElement *elem);