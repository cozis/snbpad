#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"
#include "xutf8.h"
#include "gapiter.h"
#include "textdisplay.h"

void Selection_getSlice(Selection selection,
                        size_t *offset,
                        size_t *length)
{
    assert(selection.active);
    if (selection.start < selection.end) {
        *offset = selection.start;
        *length = selection.end - selection.start;
    } else {
        *offset = selection.end;
        *length = selection.start - selection.end;
    }
}

static unsigned int 
TextDisplay_getLineHeight(TextDisplay *tdisp)
{
    if (tdisp->auto_line_height) {
        unsigned int h1 = tdisp->text.font->baseSize;
        unsigned int h2 = tdisp->lineno.font->baseSize 
                        + tdisp->lineno.padding_up 
                        + tdisp->lineno.padding_down;
        return MAX(h1, h2);
    }
    return tdisp->line_height;
}

static unsigned int 
TextDisplay_getLinenoColumnWidth(TextDisplay *tdisp)
{
    int width;
    if (tdisp->lineno.hide)
        width = 0;
    else if (tdisp->lineno.auto_width) {
        size_t max_lineno = GapBuffer_getLineno(tdisp->buffer);
        char max_lineno_as_text[8];
        snprintf(max_lineno_as_text, sizeof(max_lineno_as_text), "%ld", max_lineno);
        Font font = *tdisp->lineno.font;
        Vector2 size = MeasureTextEx(font, max_lineno_as_text, (int) font.baseSize, 0);
        width = size.x;
    } else
        width = tdisp->lineno.width;
    return width
         + tdisp->lineno.padding_left
         + tdisp->lineno.padding_right;
}

static size_t 
cursorFromClick(TextDisplay *tdisp,
                int x, int y)
{
    GapBufferIter iter;
    GapBufferIter_init(&iter, tdisp->buffer);

    Slice line;
    int line_idx = (y - tdisp->y + tdisp->v_scroll.amount) / TextDisplay_getLineHeight(tdisp);
    if (!GapBufferIter_getLine(&iter, line_idx, &line)) {
        GapBufferIter_free(&iter);
        return GapBuffer_getUsage(tdisp->buffer);
    }

    unsigned int lineno_colm_w = TextDisplay_getLinenoColumnWidth(tdisp);

    Font font = *tdisp->text.font;
    Vector2 size = MeasureTextEx(font, line.str, (int) font.baseSize, 0);
    int line_w = size.x;

    int cur;
    if (x - tdisp->x < (int) lineno_colm_w)
        cur = line.off;
    else if (x - tdisp->x > (int) lineno_colm_w + line_w)
        cur = line.off + line.len;
    else {
        int lo = 0;
        int hi = line.len;
        while (lo < hi-1) {

            // NOTE: This won't work if [m] doesn't refer
            //       to the first byte of a multibyte utf8
            //       symbol!!
            int m = (lo + hi) / 2;

            int mx;
            {
                char c = line.str[m];
                line.str[m] = '\0';
                Font font = *tdisp->text.font;
                Vector2 size = MeasureTextEx(font, line.str, (int) font.baseSize, 0);
                mx = size.x;
                line.str[m] = c;
            }

            if ((int) lineno_colm_w + mx < x - tdisp->x) 
                lo = m;
            else
                hi = m;
        }
        cur = line.off + lo;
    }
    GapBufferIter_free(&iter);
    return cur;
}

static unsigned int 
TextDisplay_getLogicalHeight(TextDisplay *tdisp)
{
    return GapBuffer_getLineno(tdisp->buffer) * TextDisplay_getLineHeight(tdisp);
}

static bool verticalScrollReachedLowerLimit(TextDisplay *tdisp)
{
    return tdisp->v_scroll.amount == 0;
}

static bool verticalScrollReachedUpperLimit(TextDisplay *tdisp)
{
    return tdisp->v_scroll.amount == (int) TextDisplay_getLogicalHeight(tdisp) - tdisp->h;
}

static void setVerticalScroll(TextDisplay *tdisp, int scroll)
{
    const int max_scroll = TextDisplay_getLogicalHeight(tdisp) - tdisp->h 
                         + TextDisplay_getLineHeight(tdisp); // Don't know why this need to be here, but it does!
    scroll = MAX(scroll, 0);
    scroll = MIN(scroll, max_scroll);
    tdisp->v_scroll.amount = scroll;
}

static void increaseVerticalScroll(TextDisplay *tdisp, int scroll_incr)
{
    setVerticalScroll(tdisp, tdisp->v_scroll.amount + scroll_incr);
}

void TextDisplay_tick(TextDisplay *tdisp)
{
    if (tdisp->v_scroll.force != 0) {
        increaseVerticalScroll(tdisp, tdisp->v_scroll.force);
        if (tdisp->v_scroll.force < 0) {
            tdisp->v_scroll.force += tdisp->v_scroll.inertia;
            if (tdisp->v_scroll.force > 0)
                tdisp->v_scroll.force = 0;
            if (verticalScrollReachedLowerLimit(tdisp))
                tdisp->v_scroll.force = 0;
        } else {
            tdisp->v_scroll.force -= tdisp->v_scroll.inertia;
            if (tdisp->v_scroll.force < 0)
                tdisp->v_scroll.force = 0;
            if (verticalScrollReachedUpperLimit(tdisp))
                tdisp->v_scroll.force = 0;
        }
    }
}

void TextDisplay_onMouseWheel(TextDisplay *tdisp, int y)
{
    tdisp->v_scroll.force += 30 * y;
}

void TextDisplay_onMouseMotion(TextDisplay *tdisp, int x, int y)
{
    if (tdisp->v_scroll.active) {
        const unsigned int logical_text_height = TextDisplay_getLogicalHeight(tdisp);
        int y_delta = y - tdisp->v_scroll.start_cursor;
        int amount = tdisp->v_scroll.start_amount + y_delta * (double) logical_text_height / tdisp->h;
        setVerticalScroll(tdisp, amount);
    } else if (tdisp->selecting) {
        size_t pos = cursorFromClick(tdisp, x, y);
        tdisp->selection.end = pos;
    }
}

static void getVerticalScrollbarPosition(TextDisplay *tdisp,
                                         Rectangle *scrollbar, 
                                         Rectangle *thumb)
{
    if (TextDisplay_getLogicalHeight(tdisp) < (unsigned int) tdisp->h) {
        scrollbar->width  = 0;
        scrollbar->height = 0;
        scrollbar->x = 0;
        scrollbar->y = 0;
        thumb->width  = 0;
        thumb->height = 0;
        thumb->x = 0;
        thumb->y = 0;
    } else {
        scrollbar->width  = tdisp->scrollbar_size;
        scrollbar->height = tdisp->h;
        scrollbar->x = tdisp->x + tdisp->w - scrollbar->width;
        scrollbar->y = tdisp->y;

        int logical_text_height = TextDisplay_getLogicalHeight(tdisp);
        int thumb_y = tdisp->h * (double) tdisp->v_scroll.amount / logical_text_height;
        int thumb_height = scrollbar->height * (double) tdisp->h / logical_text_height;

        thumb->x = scrollbar->x;
        thumb->y = scrollbar->y + thumb_y;
        thumb->width  = scrollbar->width;
        thumb->height = thumb_height;
    }
}

void TextDisplay_onClickDown(TextDisplay *tdisp, int x, int y)
{
    assert(x >= tdisp->x && x < tdisp->x + tdisp->w);
    assert(y >= tdisp->y && y < tdisp->y + tdisp->h);

    Rectangle v_scrollbar, v_thumb;
    getVerticalScrollbarPosition(tdisp, &v_scrollbar, &v_thumb);

    Vector2 cursor_point = {x, y};
    if (CheckCollisionPointRec(cursor_point, v_scrollbar)) {
        if (CheckCollisionPointRec(cursor_point, v_thumb)) {
            tdisp->v_scroll.active = true;
            tdisp->v_scroll.start_cursor = y;
            tdisp->v_scroll.start_amount = tdisp->v_scroll.amount;
        }
    } else {

        if (tdisp->selection.active) {
            tdisp->selection.active = false;
        } else if (!tdisp->selecting) {
            tdisp->selecting = true;
            tdisp->selection.active = true;
            tdisp->selection.start = cursorFromClick(tdisp, x, y);
            tdisp->selection.end = tdisp->selection.start;
        }
    }
}

void TextDisplay_onClickUp(TextDisplay *tdisp, int x, int y)
{
    if (tdisp->v_scroll.active)
        tdisp->v_scroll.active = false;
    else if (tdisp->selecting) {
        tdisp->selecting = false;
        if (tdisp->selection.start == tdisp->selection.end) {
            tdisp->selection.active = false;
            size_t cur = cursorFromClick(tdisp, x, y);
            GapBuffer_setCursor(tdisp->buffer, cur);
        }
    }
}

void TextDisplay_onArrowLeftDown(TextDisplay *tdisp)
{
    tdisp->selection.active = false;
    GapBuffer_moveCursorBackward(tdisp->buffer);
}

void TextDisplay_onArrowRightDown(TextDisplay *tdisp)
{
    tdisp->selection.active = false;
    GapBuffer_moveCursorForward(tdisp->buffer);
}

void TextDisplay_onTabDown(TextDisplay *tdisp)
{
    tdisp->selection.active = false;
    GapBuffer_insertString(tdisp->buffer, "    ", 4);                         
}

void TextDisplay_onBackspaceDown(TextDisplay *tdisp)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    } else
        GapBuffer_removeBackwards(tdisp->buffer); 
}

void TextDisplay_onReturnDown(TextDisplay *tdisp)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    }
    GapBuffer_insertString(tdisp->buffer, "\n", 1); 
}

void TextDisplay_onTextInput(TextDisplay *tdisp, 
                             const char *str, 
                             size_t len)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    }
    GapBuffer_insertString(tdisp->buffer, str, len);
}

void TextDisplay_onTextInput2(TextDisplay *tdisp, uint32_t rune)
{
    char buffer[16];
    int len = xutf8_sequence_from_utf32_codepoint(buffer, sizeof(buffer), rune);
    if (len < 0)
        TraceLog(LOG_WARNING, "RayLib provided an invalid unicode rune");
    else
        TextDisplay_onTextInput(tdisp, buffer, (size_t) len);
}

static void cutOrCopy(TextDisplay *tdisp, bool cut)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        char *s = GapBuffer_copyRange(tdisp->buffer, offset, length);
        if (s == NULL)
            TraceLog(LOG_WARNING, "Failed to copy text from the buffer");
        else {
            SetClipboardText(s);
            free(s);
        }

        if (cut) {
            GapBuffer_removeRangeAndSetCursor(tdisp->buffer, offset, length);
            tdisp->selection.active = false;
        }
    }
}

void TextDisplay_onCopy(TextDisplay *tdisp)
{
    cutOrCopy(tdisp, false);
}

void TextDisplay_onCut(TextDisplay *tdisp)
{
    cutOrCopy(tdisp, true);
}

void TextDisplay_onPaste(TextDisplay *tdisp)
{
    const char *s = GetClipboardText();
    if (s != NULL)
        GapBuffer_insertString(tdisp->buffer, s, strlen(s));
}

static size_t getSubstringRenderWidth(Slice line, Font font,
                                      size_t offset, size_t length)
{
    char c = line.str[offset + length];
    line.str[offset + length] = '\0';
    Vector2 size = MeasureTextEx(font, line.str + offset, 
                                 (int) font.baseSize, 0);
    line.str[offset + length] = c;
    return size.x;
}

static size_t getStringRenderWidth(Font font, const char *str)
{
    Vector2 size = MeasureTextEx(font, str, (int) font.baseSize, 0);
    return size.x;
}

void TextDisplay_draw(TextDisplay *tdisp)
{
    ClearBackground((Color) {
        0xee, 0xee, 0xee, 0xff
    });

    unsigned int line_height = TextDisplay_getLineHeight(tdisp);

    int y_relative = -tdisp->v_scroll.amount;
    size_t no = 1;
    GapBufferIter iter;
    GapBufferIter_init(&iter, tdisp->buffer);

    while (y_relative + (int) line_height < 0 && GapBufferIter_nextLine(&iter, NULL)) {
        y_relative += line_height;
        no++;
    }

    Slice line;
    while (y_relative < tdisp->h && GapBufferIter_nextLine(&iter, &line)) {
        
        int line_x = 0;
        int line_y = y_relative;
        int line_num_w = TextDisplay_getLinenoColumnWidth(tdisp);

        if (tdisp->lineno.hide == false) {

            if (tdisp->lineno.nobg == false)
                DrawRectangle(line_x, line_y,
                              line_num_w, line_height,
                              tdisp->lineno.bgcolor);

            char s[8];
            snprintf(s, sizeof(s), "%ld", no);
            
            Font font = *tdisp->lineno.font;
            Vector2 size = MeasureTextEx(font, s, (int) font.baseSize, 0);

            Vector2 pos;
            switch (tdisp->lineno.h_align) {
                case TextAlignH_LEFT:   pos.x = line_x + tdisp->lineno.padding_left; break;
                case TextAlignH_RIGHT:  pos.x = line_x + (line_num_w - size.x) - tdisp->lineno.padding_right;     break;
                case TextAlignH_CENTER: pos.x = line_x + (line_num_w - size.x) / 2; break;
            }
            switch (tdisp->lineno.v_align) {
                case TextAlignV_TOP:    pos.y = line_y + tdisp->lineno.padding_up; break;
                case TextAlignV_CENTER: pos.y = line_y + (line_height - size.y);     break;
                case TextAlignV_BOTTOM: pos.y = line_y + (line_height - size.y) / 2 - tdisp->lineno.padding_down; break;
            }
            DrawTextEx(font, s, pos, font.baseSize, 
                       0, tdisp->lineno.fgcolor);
        }

        if (tdisp->selection.active) {
            
            int sel_x;
            int sel_w;

            size_t selection_head;
            size_t selection_tail;
            Selection_getSlice(tdisp->selection, &selection_head, &selection_tail);
            selection_tail += selection_head;

            bool something_selected_in_line;
            
            if (line.off > selection_tail || line.off + line.len < selection_head) {
            
                /* Nothing was selected from this line */
                something_selected_in_line = false;
            
            } else if (selection_head <= line.off && line.off + line.len <= selection_tail) {

                /* Whole line is selected */
                sel_x = line_x + line_num_w;
                sel_w = getStringRenderWidth(*tdisp->text.font, line.str);
                something_selected_in_line = true;

            } else if (line.off <= selection_head && selection_tail < line.off + line.len) {
                
                /* Selection is fully contained by this line */
                int w1 = getSubstringRenderWidth(line, *tdisp->text.font, 0, selection_head - line.off);
                int w2 = getSubstringRenderWidth(line, *tdisp->text.font, 0, selection_tail - line.off);
                sel_x = line_x + line_num_w + w1;
                sel_w = w2 - w1;
                something_selected_in_line = true;

            } else if (line.off < selection_head && line.off + line.len <= selection_tail) {
                
                /* Selection goes from the middle of the line 'till the end */
                int w = getSubstringRenderWidth(line, *tdisp->text.font, 0, selection_head - line.off);
                sel_x = line_x + line_num_w + w;
                sel_w = getStringRenderWidth(*tdisp->text.font, line.str) - w;
                something_selected_in_line = true;

            } else if (selection_head < line.off && selection_tail < line.off + line.len) {

                /* Selection goes from the start to the middle */
                int w = getSubstringRenderWidth(line, *tdisp->text.font, 0, selection_tail - line.off);
                sel_x = line_x + line_num_w;
                sel_w = w;
                something_selected_in_line = true;

            } else {
                //assert(0); // All of the previous conditions
                           // must be mutually exclusive.
                something_selected_in_line = false; // Temporary
            }

            if (something_selected_in_line)
                DrawRectangle(sel_x, line_y, sel_w, line_height,
                              tdisp->text.selection_bgcolor);
            
        }

        {
            Font font = *tdisp->text.font;
            Vector2 pos = {
                .x = line_x + line_num_w, 
                .y = line_y
            };
            DrawTextEx(font, line.str, pos, 
                       font.baseSize, 0, 
                       tdisp->text.fgcolor);
        }

        {
            const size_t cursor = tdisp->buffer->gap_offset;
            if (line.off <= cursor && cursor <= line.off + line.len) {
                /* The cursor is in this line */
                int relative_cursor_x = getSubstringRenderWidth(line, *tdisp->text.font, 0, cursor - line.off);
                DrawRectangle(
                    line_x + line_num_w + relative_cursor_x,
                    line_y,
                    3,
                    line_height,
                    tdisp->cursor.bgcolor
                );
            }
        }

        y_relative += line_height;
        no++;
    }
    GapBufferIter_free(&iter);

    // Vertical scrollbar
    {
        Rectangle abs_scrollbar, abs_thumb;
        Rectangle rel_scrollbar, rel_thumb;
        getVerticalScrollbarPosition(tdisp, &abs_scrollbar, &abs_thumb);

        rel_scrollbar = abs_scrollbar;
        rel_scrollbar.x -= tdisp->x;
        rel_scrollbar.y -= tdisp->y;
        rel_thumb = abs_thumb;
        rel_thumb.x -= tdisp->x;
        rel_thumb.y -= tdisp->y;
        DrawRectangle(rel_thumb.x, rel_thumb.y,
                      rel_thumb.width, rel_thumb.height,
                      RED);
    }
}