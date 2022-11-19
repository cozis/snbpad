#include "xutf8.h"
#include "guielement.h"

bool GUIElement_allowsFocus(GUIElement *elem)
{
    return elem->methods->allows_focus;
}

void GUIElement_free(GUIElement *elem)
{
    if (elem->methods->free != NULL)
        elem->methods->free(elem);
}

void GUIElement_tick(GUIElement *elem, uint64_t time)
{
    if (elem->methods->tick != NULL)
        elem->methods->tick(elem, time);
}

void GUIElement_draw(GUIElement *elem)
{
    if (elem->methods->draw != NULL)
        elem->methods->draw(elem);
}

void GUIElement_clickUp(GUIElement *elem, 
                        int x, int y)
{
    if (elem->methods->clickUp != NULL)
        elem->methods->clickUp(elem, x, y);
}

void GUIElement_onClickDown(GUIElement *elem, 
                            int x, int y)
{
    if (elem->methods->onClickDown != NULL)
        elem->methods->onClickDown(elem, x, y);
}

void GUIElement_offClickDown(GUIElement *elem)
{
    if (elem->methods->offClickDown != NULL)
        elem->methods->offClickDown(elem);
}

void GUIElement_onMouseWheel(GUIElement *elem, int y)
{
    if (elem->methods->onMouseWheel != NULL)
        elem->methods->onMouseWheel(elem, y);
}

void GUIElement_onMouseMotion(GUIElement *elem, 
                              int x, int y)
{
    if (elem->methods->onMouseMotion != NULL)
        elem->methods->onMouseMotion(elem, x, y);
}

void GUIElement_onArrowLeftDown(GUIElement *elem)
{
    if (elem->methods->onArrowLeftDown != NULL)
        elem->methods->onArrowLeftDown(elem);
}

void GUIElement_onArrowRightDown(GUIElement *elem)
{
    if (elem->methods->onArrowRightDown != NULL)
        elem->methods->onArrowRightDown(elem);
}

void GUIElement_onReturnDown(GUIElement *elem)
{
    if (elem->methods->onReturnDown != NULL)
        elem->methods->onReturnDown(elem);
}

void GUIElement_onBackspaceDown(GUIElement *elem)
{
    if (elem->methods->onBackspaceDown != NULL)
        elem->methods->onBackspaceDown(elem);
}

void GUIElement_onTabDown(GUIElement *elem)
{
    if (elem->methods->onTabDown != NULL)
        elem->methods->onTabDown(elem);
}

void GUIElement_onTextInput(GUIElement *elem, 
                            const char *str, 
                            size_t len)
{
    if (elem->methods->onTextInput != NULL)
        elem->methods->onTextInput(elem, str, len);
}

void GUIElement_onTextInput2(GUIElement *elem, 
                             uint32_t rune)
{
    char buffer[16]; // Bigger than any single codepoint.
    int len = xutf8_sequence_from_utf32_codepoint(buffer, sizeof(buffer), rune);
    if (len < 0)
        TraceLog(LOG_WARNING, "RayLib provided an invalid unicode rune");
    else {
        buffer[len] = '\0';
        GUIElement_onTextInput(elem, buffer, (size_t) len);
    }
}

void GUIElement_onPaste(GUIElement *elem)
{
    if (elem->methods->onPaste != NULL)
        elem->methods->onPaste(elem);
}

void GUIElement_onCopy(GUIElement *elem)
{
    if (elem->methods->onCopy != NULL)
        elem->methods->onCopy(elem);
}

void GUIElement_onCut(GUIElement *elem)
{
    if (elem->methods->onCut != NULL)
        elem->methods->onCut(elem);
}

void GUIElement_onSave(GUIElement *elem)
{
    if (elem->methods->onSave != NULL)
        elem->methods->onSave(elem);
}

void GUIElement_onOpen(GUIElement *elem)
{
    if (elem->methods->onOpen != NULL)
        elem->methods->onOpen(elem);
}

void GUIElement_onFocusLost(GUIElement *elem)
{
    if (elem->methods->onFocusLost != NULL)
        elem->methods->onFocusLost(elem);
}

void GUIElement_onFocusGained(GUIElement *elem)
{
    if (elem->methods->onFocusGained != NULL)
        elem->methods->onFocusGained(elem);
}
