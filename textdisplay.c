#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "sfd.h"
#include "utils.h"
#include "xutf8.h"
#include "gapiter.h"
#include "textdisplay.h"

typedef struct {
    bool active;
    size_t start, end;
} Selection;

typedef struct {
    GUIElement base;
    const TextDisplayStyle *style;
    struct {
        int  force;
        int  amount;
        bool active;
        int start_amount;
        int start_cursor;
    } v_scroll;
    bool      selecting;
    Selection selection;
    GapBuffer buffer;
    char file[1024];
} TextDisplay;

/*
typedef struct {
    Slice *line;
    size_t cur;
} LineTagger;

typedef struct {
    Color fgcolor;
    Color bgcolor;
    size_t offset;
    size_t length;
} Tag;

void LineTagger_init(LineTagger *tagger, Slice *line);
void LineTagger_free(LineTagger *tagger);
bool LineTagger_next(LineTagger *tagger, Tag *tag);

void 
*/

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
    if (tdisp->style->auto_line_height) {
        unsigned int h1 = tdisp->style->text.font->baseSize;
        unsigned int h2 = tdisp->style->lineno.font->baseSize 
                        + tdisp->style->lineno.padding_up 
                        + tdisp->style->lineno.padding_down;
        return MAX(h1, h2);
    }
    return tdisp->style->line_height;
}

static unsigned int 
TextDisplay_getLinenoColumnWidth(TextDisplay *tdisp)
{
    int width;
    if (tdisp->style->lineno.hide)
        width = 0;
    else if (tdisp->style->lineno.auto_width) {
        size_t max_lineno = GapBuffer_getLineno(&tdisp->buffer);
        char max_lineno_as_text[8];
        snprintf(max_lineno_as_text, sizeof(max_lineno_as_text), "%ld", max_lineno);
        Font font = *tdisp->style->lineno.font;
        Vector2 size = MeasureTextEx(font, max_lineno_as_text, (int) font.baseSize, 0);
        width = size.x;
    } else
        width = tdisp->style->lineno.width;
    return width
         + tdisp->style->lineno.padding_left
         + tdisp->style->lineno.padding_right;
}

static size_t 
longestSubstringThatRendersInLessPixelsThan(Font font, 
                                            const char *str, 
                                            size_t len, 
                                            size_t max_px_len)
{
    if (str == NULL)
        str = "";

    size_t w = 0;
    size_t i = 0;
    while (i < len) {

        int consumed;
        int letter = GetCodepoint(str + i, &consumed);
        int glyph_index = GetGlyphIndex(font, letter);

        if (letter == 0x3f)
            i++;
        else
            i += consumed;

        int delta;

        int advanceX = font.glyphs[glyph_index].advanceX;
        if (advanceX)
            delta = advanceX;
        else
            delta = font.recs[glyph_index].width 
                  + font.glyphs[glyph_index].offsetX;
        
        assert(delta >= 0);
        if (w + delta > max_px_len)
            break;

        w += delta;
    }
    return i;
}

static size_t 
cursorFromClick(TextDisplay *tdisp,
                int x, int y)
{
    GapBufferIter iter;
    GapBufferIter_init(&iter, &tdisp->buffer);

    Slice line;
    int line_idx = (y + tdisp->v_scroll.amount) / TextDisplay_getLineHeight(tdisp);
    if (!GapBufferIter_getLine(&iter, line_idx, &line)) {
        GapBufferIter_free(&iter);
        return GapBuffer_getUsage(&tdisp->buffer);
    }

    unsigned int lineno_colm_w = TextDisplay_getLinenoColumnWidth(tdisp);

    Font font = *tdisp->style->text.font;
    size_t cursor;
    if (x < (int) lineno_colm_w)
        cursor = line.off;
    else
        cursor = line.off + longestSubstringThatRendersInLessPixelsThan(font, line.str, line.len, x - lineno_colm_w);

    GapBufferIter_free(&iter);
    return cursor;
}

static unsigned int 
TextDisplay_getLogicalHeight(TextDisplay *tdisp)
{
    return GapBuffer_getLineno(&tdisp->buffer) * TextDisplay_getLineHeight(tdisp);
}

static bool verticalScrollReachedLowerLimit(TextDisplay *tdisp)
{
    return tdisp->v_scroll.amount == 0;
}

static bool verticalScrollReachedUpperLimit(TextDisplay *tdisp)
{
    return tdisp->v_scroll.amount == (int) TextDisplay_getLogicalHeight(tdisp) - tdisp->base.region.height;
}

static void setVerticalScroll(TextDisplay *tdisp, int scroll)
{
    const int max_scroll = TextDisplay_getLogicalHeight(tdisp) - tdisp->base.region.height;
    scroll = MAX(scroll, 0);
    scroll = MIN(scroll, max_scroll);
    tdisp->v_scroll.amount = scroll;
}

static void increaseVerticalScroll(TextDisplay *tdisp, int scroll_incr)
{
    setVerticalScroll(tdisp, tdisp->v_scroll.amount + scroll_incr);
}

static void tickCallback(TextDisplay *tdisp)
{
    if (tdisp->v_scroll.force != 0) {
        increaseVerticalScroll(tdisp, tdisp->v_scroll.force);
        if (tdisp->v_scroll.force < 0) {
            tdisp->v_scroll.force += tdisp->style->scroll.inertia;
            if (tdisp->v_scroll.force > 0)
                tdisp->v_scroll.force = 0;
            if (verticalScrollReachedLowerLimit(tdisp))
                tdisp->v_scroll.force = 0;
        } else {
            tdisp->v_scroll.force -= tdisp->style->scroll.inertia;
            if (tdisp->v_scroll.force < 0)
                tdisp->v_scroll.force = 0;
            if (verticalScrollReachedUpperLimit(tdisp))
                tdisp->v_scroll.force = 0;
        }
    }
}

static void onMouseWheelCallback(TextDisplay *tdisp, int y)
{
    tdisp->v_scroll.force += 30 * y;
}

static void onMouseMotionCallback(TextDisplay *tdisp, 
                                  int x, int y)
{
    if (tdisp->v_scroll.active) {
        const unsigned int logical_text_height = TextDisplay_getLogicalHeight(tdisp);
        int y_delta = y - tdisp->v_scroll.start_cursor;
        int amount = tdisp->v_scroll.start_amount + y_delta * (double) logical_text_height / tdisp->base.region.height;
        setVerticalScroll(tdisp, amount);
    } else if (tdisp->selecting) {
        size_t pos = cursorFromClick(tdisp, x, y);
        tdisp->selection.end = pos;
    }
}

static void 
getVerticalScrollbarPosition(TextDisplay *tdisp,
                             Rectangle *scrollbar, 
                             Rectangle *thumb)
{
    if (TextDisplay_getLogicalHeight(tdisp) < (unsigned int) tdisp->base.region.height) {
        scrollbar->width  = 0;
        scrollbar->height = 0;
        scrollbar->x = 0;
        scrollbar->y = 0;
        thumb->width  = 0;
        thumb->height = 0;
        thumb->x = 0;
        thumb->y = 0;
    } else {
        scrollbar->width  = tdisp->style->scrollbar_size;
        scrollbar->height = tdisp->base.region.height;
        scrollbar->x = tdisp->base.region.width - scrollbar->width;
        scrollbar->y = 0;

        int logical_text_height = TextDisplay_getLogicalHeight(tdisp);
        int thumb_y = tdisp->base.region.height * (double) tdisp->v_scroll.amount / logical_text_height;
        int thumb_height = scrollbar->height * (double) tdisp->base.region.height / logical_text_height;

        thumb->x = scrollbar->x;
        thumb->y = scrollbar->y + thumb_y;
        thumb->width  = scrollbar->width;
        thumb->height = thumb_height;
    }
}

static void onClickDownCallback(TextDisplay *tdisp, 
                                int x, int y)
{
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
            TraceLog(LOG_INFO, "Selection started");
            tdisp->selecting = true;
            tdisp->selection.active = true;
            tdisp->selection.start = cursorFromClick(tdisp, x, y);
            tdisp->selection.end = tdisp->selection.start;
        }
    }
}

static void offClickDownCallback(TextDisplay *tdisp)
{
    (void) tdisp;    
}

static void clickUpCallback(TextDisplay *tdisp, int x, int y)
{
    if (tdisp->v_scroll.active)
        tdisp->v_scroll.active = false;
    else if (tdisp->selecting) {
        TraceLog(LOG_INFO, "Selection stopped");
        tdisp->selecting = false;
        if (tdisp->selection.start == tdisp->selection.end) {
            tdisp->selection.active = false;
            size_t cur = cursorFromClick(tdisp, x, y);
            GapBuffer_setCursor(&tdisp->buffer, cur);
        }
    }
}

static void onArrowLeftDownCallback(TextDisplay *tdisp)
{
    tdisp->selection.active = false;
    GapBuffer_moveCursorBackward(&tdisp->buffer);
}

static void onArrowRightDownCallback(TextDisplay *tdisp)
{
    tdisp->selection.active = false;
    GapBuffer_moveCursorForward(&tdisp->buffer);
}

static void onTabDownCallback(TextDisplay *tdisp)
{
    tdisp->selection.active = false;
    GapBuffer_insertString(&tdisp->buffer, "    ", 4);                         
}

static void onBackspaceDownCallback(TextDisplay *tdisp)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    } else
        GapBuffer_removeBackwards(&tdisp->buffer); 
}

static void onReturnDownCallback(TextDisplay *tdisp)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    }
    GapBuffer_insertString(&tdisp->buffer, "\n", 1); 
}

static void onTextInputCallback(TextDisplay *tdisp, 
                                const char *str, 
                                size_t len)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    }
    GapBuffer_insertString(&tdisp->buffer, str, len);
}

static void cutOrCopy(TextDisplay *tdisp, bool cut)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        char *s = GapBuffer_copyRange(&tdisp->buffer, offset, length);
        if (s == NULL)
            TraceLog(LOG_WARNING, "Failed to copy text from the buffer");
        else {
            SetClipboardText(s);
            free(s);
        }

        if (cut) {
            GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
            tdisp->selection.active = false;
        }
    }
}

static void onCopyCallback(TextDisplay *tdisp)
{
    cutOrCopy(tdisp, false);
}

static void onCutCallback(TextDisplay *tdisp)
{
    cutOrCopy(tdisp, true);
}

static void onPasteCallback(TextDisplay *tdisp)
{
    const char *s = GetClipboardText();
    if (s != NULL)
        GapBuffer_insertString(&tdisp->buffer, s, strlen(s));
}

static void onOpenCallback(TextDisplay *tdisp)
{
    sfd_Options opt = {
        .title        = "Open Text File",
        .filter_name  = "Text File",
        .filter       = "*.txt|*.c|*.h",
    };

    const char *file = sfd_open_dialog(&opt);

    if (file == NULL) {
        TraceLog(LOG_ERROR, "Failed to get dialog result");
        return;
    }

    GapBuffer temp;
    if (GapBuffer_initFile(&temp, file)) {
        /* Managed to open the file in a buffer */

        // Swap the current one with the new one
        GapBuffer_free(&tdisp->buffer);
        tdisp->buffer = temp;

        strncpy(tdisp->file, file, sizeof(tdisp->file));
    }
}

static void onSaveCallback(TextDisplay *tdisp)
{
    if (tdisp->file[0] == '\0') {
        
        sfd_Options opt = {
            .title        = "Save Text File",
            .filter_name  = "Text File",
            .filter       = "*.txt|*.c|*.h",
        };

        const char *file = sfd_save_dialog(&opt);

        if (file == NULL) {
            TraceLog(LOG_ERROR, "Failed to get dialog result");
            return;
        }
        strncpy(tdisp->file, file, sizeof(tdisp->file));
    }        

    FILE *stream = fopen(tdisp->file, "wb");
    if (stream == NULL) {
        TraceLog(LOG_ERROR, "Failed to open \"%s\" in write mode\n", tdisp->file);
    } else {
        if (!GapBuffer_saveToStream(&tdisp->buffer, stream))
            TraceLog(LOG_ERROR, "Failed to save to \"%s\"\n", tdisp->file);
        fclose(stream);
    }
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

typedef struct {
    TextDisplay *tdisp;
    GapBufferIter iter;
    unsigned int line_height;
    int line_num_w;
    int line_x;
    int line_y;
    Slice line;
    unsigned int no;
} DrawContext;

static void initDrawContext(DrawContext *draw_context, 
                            TextDisplay *tdisp)
{
    draw_context->tdisp = tdisp;
    draw_context->line_height = TextDisplay_getLineHeight(tdisp);
    draw_context->line_num_w  = TextDisplay_getLinenoColumnWidth(tdisp);
    draw_context->line_x = 0;
    draw_context->line_y = -tdisp->v_scroll.amount;
    draw_context->no = 0;
    GapBufferIter_init(&draw_context->iter, &tdisp->buffer);
}

static void freeDrawContext(DrawContext *draw_context)
{
    GapBufferIter_free(&draw_context->iter);
}

static void skipLinesBeforeViewport(DrawContext *draw_context)
{
    while (draw_context->line_y + (int) draw_context->line_height < 0 
        && GapBufferIter_nextLine(&draw_context->iter, NULL)) {
        draw_context->line_y += draw_context->line_height;
        draw_context->no++;
    }
}

static bool nextLine(DrawContext *draw_context)
{
    if (draw_context->no == 0)
        skipLinesBeforeViewport(draw_context);
    else
        draw_context->line_y += draw_context->line_height;
    draw_context->no++;
    bool line_starts_after_viewport = draw_context->line_y > draw_context->tdisp->base.region.height;
    bool no_more_lines_are_left = !GapBufferIter_nextLine(&draw_context->iter, &draw_context->line);
    bool done = line_starts_after_viewport || no_more_lines_are_left;
    return !done;
}

static void drawLineno(DrawContext draw_context)
{
    TextDisplay *tdisp = draw_context.tdisp;

    if (tdisp->style->lineno.hide == false) {
        
        if (tdisp->style->lineno.nobg == false)
            DrawRectangle(draw_context.line_x,
                          draw_context.line_y,
                          draw_context.line_num_w, 
                          draw_context.line_height,
                          tdisp->style->lineno.bgcolor);

        char s[8];
        snprintf(s, sizeof(s), "%d", draw_context.no);
        
        Font font = *tdisp->style->lineno.font;
        Vector2 size = MeasureTextEx(font, s, (int) font.baseSize, 0);

        Vector2 pos;
        switch (tdisp->style->lineno.h_align) {
            case TextAlignH_LEFT:   pos.x = draw_context.line_x + tdisp->style->lineno.padding_left; break;
            case TextAlignH_RIGHT:  pos.x = draw_context.line_x + (draw_context.line_num_w - size.x) - tdisp->style->lineno.padding_right; break;
            case TextAlignH_CENTER: pos.x = draw_context.line_x + (draw_context.line_num_w - size.x) / 2; break;
        }
        switch (tdisp->style->lineno.v_align) {
            case TextAlignV_TOP:    pos.y = draw_context.line_y + tdisp->style->lineno.padding_up; break;
            case TextAlignV_CENTER: pos.y = draw_context.line_y + (draw_context.line_height - size.y); break;
            case TextAlignV_BOTTOM: pos.y = draw_context.line_y + (draw_context.line_height - size.y) / 2 - tdisp->style->lineno.padding_down; break;
        }
        DrawTextEx(font, s, pos, font.baseSize, 
                   0, tdisp->style->lineno.fgcolor);
    }
}

static void drawSelection(DrawContext draw_context)
{
    TextDisplay *tdisp = draw_context.tdisp;
    Slice line = draw_context.line;

    if (tdisp->selection.active) {

        size_t sel_abs_off, sel_len;
        Selection_getSlice(tdisp->selection, &sel_abs_off, &sel_len);
        int sel_rel_off = sel_abs_off - line.off;

        if (sel_abs_off < line.off + line.len && sel_abs_off + sel_len > line.off) {

            size_t rel_head = MAX(sel_rel_off, 0);
            size_t rel_tail = MIN(sel_rel_off + sel_len, line.len);
            
            Font font = *tdisp->style->text.font;
            int sel_w = getSubstringRenderWidth(line, font, rel_head, rel_tail - rel_head);
            int sel_x = getSubstringRenderWidth(line, font, 0,        rel_head)
                      + draw_context.line_x + draw_context.line_num_w;
            DrawRectangle(
                sel_x, draw_context.line_y, 
                sel_w, draw_context.line_height,
                tdisp->style->text.selection_bgcolor);
        }
    }
}

static void drawLineText(DrawContext draw_context)
{
    TextDisplay *tdisp = draw_context.tdisp;
    Font font = *tdisp->style->text.font;
    Vector2 pos = {
        .x = draw_context.line_x + draw_context.line_num_w, 
        .y = draw_context.line_y
    };
    DrawTextEx(font, draw_context.line.str, pos, 
               font.baseSize, 0, 
               tdisp->style->text.fgcolor);
}

static void drawCursor(DrawContext draw_context)
{
    TextDisplay *tdisp = draw_context.tdisp;
    const size_t cursor = tdisp->buffer.gap_offset;
    Slice line = draw_context.line;
    Font font = *tdisp->style->text.font;

    if (line.off <= cursor && cursor <= line.off + line.len) {
        /* The cursor is in this line */
        int relative_cursor_x = getSubstringRenderWidth(line, font, 0, cursor - line.off);
        DrawRectangle(
            draw_context.line_x + draw_context.line_num_w + relative_cursor_x,
            draw_context.line_y,
            3,
            draw_context.line_height,
            tdisp->style->cursor.bgcolor
        );
    }
}

static void drawVerticalScrollbar(DrawContext draw_context)
{
    Rectangle scrollbar, thumb;
    getVerticalScrollbarPosition(draw_context.tdisp, 
                                 &scrollbar, &thumb);
    DrawRectangle(thumb.x, thumb.y,
                  thumb.width, thumb.height,
                  RED);
}

static void drawCallback(TextDisplay *tdisp)
{
    ClearBackground(tdisp->style->text.bgcolor);

    DrawContext draw_context;
    initDrawContext(&draw_context, tdisp);
    while (nextLine(&draw_context)) {
        drawLineno(draw_context);
        drawSelection(draw_context);
        drawLineText(draw_context);
        drawCursor(draw_context);
    }
    drawVerticalScrollbar(draw_context);
    freeDrawContext(&draw_context);
}

static void freeCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    GapBuffer_free(&tdisp->buffer);
    free(elem);
}

static const GUIElementMethods methods = {
    .allows_focus = true,
    .free = freeCallback,
    .tick = tickCallback,
    .draw = drawCallback,
    .clickUp = clickUpCallback,
    .onClickDown = onClickDownCallback,
    .offClickDown = offClickDownCallback,
    .onMouseWheel = onMouseWheelCallback,
    .onMouseMotion = onMouseMotionCallback,
    .onArrowLeftDown = onArrowLeftDownCallback,
    .onArrowRightDown = onArrowRightDownCallback,
    .onReturnDown = onReturnDownCallback,
    .onBackspaceDown = onBackspaceDownCallback,
    .onTabDown = onTabDownCallback,
    .onTextInput = onTextInputCallback,
    .onPaste = onPasteCallback,
    .onCopy = onCopyCallback,
    .onCut = onCutCallback,
    .onSave = onSaveCallback,
    .onOpen = onOpenCallback,
};

GUIElement *TextDisplay_new(Rectangle region,
                            const char *name,
                            const char *file,
                            const TextDisplayStyle *style)
{
    TextDisplay *tdisp = malloc(sizeof(TextDisplay));
    if (tdisp != NULL) {
        tdisp->base.region = region;
        tdisp->base.methods = &methods;
        strncpy(tdisp->base.name, name, 
                sizeof(tdisp->base.name));
        tdisp->style = style;
        tdisp->v_scroll.active = false;
        tdisp->v_scroll.force = 0;
        tdisp->v_scroll.amount = 0;
        tdisp->v_scroll.start_amount = 0;
        tdisp->v_scroll.start_cursor = 0;
        tdisp->selecting = false;
        tdisp->selection.active = false;
        
        if (file == NULL) {
            tdisp->file[0] = '\0';
            GapBuffer_initEmpty(&tdisp->buffer);
        } else {
            strncpy(tdisp->file, file, sizeof(tdisp->file));
            if (FileExists(file)) {
                if (!GapBuffer_initFile(&tdisp->buffer, file)) {
                    TraceLog(LOG_WARNING, "Failed to load \"%s\"", file);
                    GapBuffer_initEmpty(&tdisp->buffer);
                }
            } else
                GapBuffer_initEmpty(&tdisp->buffer);
        }
    }
    return (GUIElement*) tdisp;
}
