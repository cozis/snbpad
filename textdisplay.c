#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "sfd.h"
#include "utils.h"
#include "xutf8.h"
#include "gapiter.h"
#include "textdisplay.h"
#include "textrenderutils.h"

typedef struct {
    bool active;
    size_t start, end;
} Selection;

typedef struct {
    GUIElement base;
    Rectangle old_region;
    const TextDisplayStyle *style;
    struct {
        Font font;
    } text;
    struct {
        Font font;
    } lineno;
    struct {
        int  force;
        int  amount;
        bool active;
        int start_amount;
        int start_cursor;
    } v_scroll;
    struct {
        double intensity;
    } cursor;
    RenderTexture2D texture;
    bool      focused;
    bool      selecting;
    Selection selection;
    GapBuffer buffer;
    char file[1024];
} TextDisplay;

static void updateWindowTitle(TextDisplay *tdisp)
{
    char buffer[256];
    if (tdisp->file[0] == '\0')
        strncpy(buffer, "SnBpad - (unnamed)", sizeof(buffer));
    else
        snprintf(buffer, sizeof(buffer), "SnBpad - %s", tdisp->file);
    SetWindowTitle(buffer);
}

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
        unsigned int h1 = tdisp->style->text.font_size;
        unsigned int h2 = tdisp->style->lineno.font_size 
                        + tdisp->style->lineno.padding_up 
                        + tdisp->style->lineno.padding_down;
        return MAX(h1, h2);
    }
    return tdisp->style->line_height;
}

static float
TextDisplay_getLinenoColumnWidth(TextDisplay *tdisp)
{
    float width;
    if (tdisp->style->lineno.hide)
        width = 0;
    else if (tdisp->style->lineno.auto_width) {
        Font font = tdisp->lineno.font;
        size_t max_lineno = GapBuffer_getLineno(&tdisp->buffer);
        char s[8];
        int n = snprintf(s, sizeof(s), "%ld", max_lineno);
        assert(n >= 0 && n < (int) sizeof(s));
        width = calculateStringRenderWidth(font, tdisp->style->lineno.font_size, s, n);
    } else
        width = tdisp->style->lineno.width;
    return width
         + tdisp->style->lineno.padding_left
         + tdisp->style->lineno.padding_right;
}

static size_t 
cursorFromClick(TextDisplay *tdisp,
                float x, float y)
{
    GapBufferIter iter;
    GapBufferIter_init(&iter, &tdisp->buffer);
    
    Line line;
    int line_idx = (y + tdisp->v_scroll.amount) / TextDisplay_getLineHeight(tdisp);
    if (!GapBufferIter_getLine(&iter, line_idx, &line)) {
        GapBufferIter_free(&iter);
        return GapBuffer_getUsage(&tdisp->buffer);
    }

    float lineno_colm_w = TextDisplay_getLinenoColumnWidth(tdisp);

    Font font = tdisp->text.font;
    size_t cursor;
    if (x < lineno_colm_w)
        cursor = line.off;
    else
        cursor = line.off + longestSubstringThatRendersInLessPixelsThan(font, tdisp->style->text.font_size, line.str, line.len, x - lineno_colm_w);

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
    int logical_height = TextDisplay_getLogicalHeight(tdisp);

    int max_scroll;
    if (logical_height > tdisp->base.region.height)
        max_scroll = logical_height - tdisp->base.region.height;
    else
        max_scroll = 0;

    scroll = MAX(scroll, 0);
    scroll = MIN(scroll, max_scroll);
    tdisp->v_scroll.amount = scroll;
}

static void increaseVerticalScroll(TextDisplay *tdisp, int scroll_incr)
{
    setVerticalScroll(tdisp, tdisp->v_scroll.amount + scroll_incr);
}

static void getMinimumSize(GUIElement *elem, 
                           int *w, int *h)
{
    *w = 50;
    *h = 50;
}

static void onResizeCallback(GUIElement *elem, 
                             Rectangle old_region)
{
    (void) old_region;
    Rectangle region = GUIElement_getRegion(elem);
    TextDisplay *td = (TextDisplay*) elem;
    UnloadRenderTexture(td->texture);
    td->texture = LoadRenderTexture(region.width, region.height);
}

static void tickCallback(GUIElement *elem, uint64_t time_in_ms)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

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

static void onMouseWheelCallback(GUIElement *elem, int y)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    tdisp->v_scroll.force += 30 * y;
}

static void onMouseMotionCallback(GUIElement *elem, 
                                  int x, int y)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

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

static bool 
clickedOnScrollbarThumb(TextDisplay *td, 
                        int x, int y)
{
    Rectangle v_scrollbar, v_thumb;
    getVerticalScrollbarPosition(td, &v_scrollbar, &v_thumb);

    Vector2 cursor_point = {x, y};
    if (CheckCollisionPointRec(cursor_point, v_scrollbar))
        /* Clicked on scrollbar */
        if (CheckCollisionPointRec(cursor_point, v_thumb))
            /* Clicked on thumb */
            return true;
    return false;
}

static GUIElement*
onClickDownCallback(GUIElement *elem, 
                    int x, int y)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    bool on_thumb = clickedOnScrollbarThumb(tdisp, x, y);

    if (on_thumb) {
        tdisp->v_scroll.active = true;
        tdisp->v_scroll.start_cursor = y;
        tdisp->v_scroll.start_amount = tdisp->v_scroll.amount;
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

    if (on_thumb)
        return NULL;
    else
        return elem;
}

static void offClickDownCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    (void) tdisp;    
}

static void clickUpCallback(GUIElement *elem, 
                            int x, int y)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

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

static void onArrowLeftDownCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    tdisp->selection.active = false;
    GapBuffer_moveCursorBackward(&tdisp->buffer);
}

static void onArrowRightDownCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    tdisp->selection.active = false;
    GapBuffer_moveCursorForward(&tdisp->buffer);
}

static void onBackspaceDownCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    } else
        GapBuffer_removeBackwards(&tdisp->buffer); 
}

static void onReturnDownCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    }
    GapBuffer_insertString(&tdisp->buffer, "\n", 1); 
}

static void onTextInputCallback(GUIElement *elem, 
                                const char *str, 
                                size_t len)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        GapBuffer_removeRangeAndSetCursor(&tdisp->buffer, offset, length);
        tdisp->selection.active = false;
    }
    GapBuffer_insertString(&tdisp->buffer, str, len);
}

static void onFocusLost(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    tdisp->focused = false;
}

static void onFocusGained(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    tdisp->focused = true;
    updateWindowTitle(tdisp);
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

static void onCopyCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    cutOrCopy(tdisp, false);
}

static void onCutCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    cutOrCopy(tdisp, true);
}

static void onPasteCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    const char *s = GetClipboardText();
    if (s != NULL)
        GapBuffer_insertString(&tdisp->buffer, s, strlen(s));
}

static void onOpenCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
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
        tdisp->v_scroll.amount = 0;
        strncpy(tdisp->file, file, sizeof(tdisp->file));
        updateWindowTitle(tdisp);
    }
}

static void onSaveCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
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
        updateWindowTitle(tdisp);
    }        

    FILE *stream = fopen(tdisp->file, "wb");
    if (stream == NULL) {
        TraceLog(LOG_ERROR, "Failed to open \"%s\" in write mode", tdisp->file);
    } else {
        if (!GapBuffer_saveToStream(&tdisp->buffer, stream))
            TraceLog(LOG_ERROR, "Failed to save to \"%s\"\n", tdisp->file);
        fclose(stream);
    }
}

static bool openFileCallback(GUIElement *elem, 
                             const char *file)
{
    TextDisplay *td = (TextDisplay*) elem;
    
    bool opened = false;

    size_t file_len = strlen(file);

    if (file_len >= sizeof(td->file))
        // This file name is too long to be
        // held by the widget.
        TraceLog(LOG_ERROR, "File name is too long to be stored");
    else {

        FILE *stream = fopen(file, "rb");
        if (stream == NULL)
            TraceLog(LOG_ERROR, "Failed to open \"%s\" in read mode", file);
        else {
            GapBuffer buffer2;
            if (!GapBuffer_initFile(&buffer2, file)) 
                TraceLog(LOG_ERROR, "Failed to insert \"%s\" into the gap buffer", file);
            else {
                GapBuffer_free(&td->buffer);
                td->buffer = buffer2;
                td->v_scroll.amount = 0;
                strcpy(td->file, file);
                TraceLog(LOG_INFO, "Opened file \"%s\"", file);
                opened = true;
            }
            fclose(stream);
        }
    }
    return opened;
}

typedef struct {
    TextDisplay *tdisp;
    GapBufferIter iter;
    unsigned int line_height;
    float line_num_w;
    int line_x;
    int line_y;
    Line line;
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

static void drawLineno(int no, int x, int y, 
                       int w, int h, Font font,
                       const TextDisplayStyle *style)
{
    if (style->lineno.hide == false) {
        
        if (style->lineno.nobg == false)
            DrawRectangle(x, y, w, h, style->lineno.bgcolor);

        char s[8];
        int n = snprintf(s, sizeof(s), "%d", no);
        assert(n >= 0 && n < (int) sizeof(s));

        int text_h = style->lineno.font_size;
        int text_w = calculateStringRenderWidth(font, style->lineno.font_size, s, n);
        int text_x;
        int text_y;
        switch (style->lineno.h_align) {
            case TextAlignH_LEFT:   text_x = x + style->lineno.padding_left; break;
            case TextAlignH_RIGHT:  text_x = x + (w - text_w) - style->lineno.padding_right; break;
            case TextAlignH_CENTER: text_x = x + (w - text_w) / 2; break;
        }
        switch (style->lineno.v_align) {
            case TextAlignV_TOP:    text_y = y + style->lineno.padding_up; break;
            case TextAlignV_CENTER: text_y = y + (h - text_h) / 2; break;
            case TextAlignV_BOTTOM: text_y = y + (h - text_h) - style->lineno.padding_down; break;
        }
        renderString(font, s, n, text_x, text_y, 
                     style->lineno.font_size, 
                     style->lineno.fgcolor);
    }
}

static void drawSelection(DrawContext draw_context)
{
    TextDisplay *tdisp = draw_context.tdisp;
    Line line = draw_context.line;

    if (tdisp->selection.active) {

        size_t sel_abs_off, sel_len;
        Selection_getSlice(tdisp->selection, &sel_abs_off, &sel_len);
        int sel_rel_off = sel_abs_off - line.off;

        if (sel_abs_off < line.off + line.len && sel_abs_off + sel_len > line.off) {

            int sel_w;
            int sel_x;
            if (line.len == 0) {
                sel_w = 10;
                sel_x = draw_context.line_x 
                      + draw_context.line_num_w;;
            } else {
                size_t rel_head = MAX(sel_rel_off, 0);
                size_t rel_tail = MIN(sel_rel_off + sel_len, line.len);
            
                Font font = tdisp->text.font;
                sel_w = calculateStringRenderWidth(font, tdisp->style->text.font_size, line.str + rel_head, rel_tail - rel_head);
                sel_x = calculateStringRenderWidth(font, tdisp->style->text.font_size, line.str, rel_head)
                      + draw_context.line_x + draw_context.line_num_w;
            }
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
    Font font = tdisp->text.font;
    int  font_size = tdisp->style->text.font_size;
    const char  *s = draw_context.line.str;
    const size_t n = draw_context.line.len;
    int x = draw_context.line_x + draw_context.line_num_w;
    int y = draw_context.line_y + (draw_context.line_height - font_size) / 2;
    renderString(font, s, n, x, y, font_size, 
                 tdisp->style->text.fgcolor);
}

static bool drawCursor(DrawContext draw_context)
{
    TextDisplay *tdisp = draw_context.tdisp;
    const size_t cursor = tdisp->buffer.gap_offset;
    Line line = draw_context.line;
    Font font = tdisp->text.font;

    if (line.off <= cursor && cursor <= line.off + line.len) {
        /* The cursor is in this line */
        if (tdisp->focused) {
            int relative_cursor_x = calculateStringRenderWidth(font, tdisp->style->text.font_size, line.str, cursor - line.off);
            Color color = tdisp->style->cursor.bgcolor;
            DrawRectangle(
                draw_context.line_x + draw_context.line_num_w + relative_cursor_x,
                draw_context.line_y,
                3,
                draw_context.line_height,
                color
            );
        }
        return true;
    }
    return false;
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

static void drawCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;

    BeginTextureMode(tdisp->texture);
    ClearBackground(tdisp->style->text.bgcolor);
    
    bool drew_cursor = false;
    DrawContext draw_context;
    initDrawContext(&draw_context, tdisp);
    while (nextLine(&draw_context)) {
        drawLineno(draw_context.no, 
                   draw_context.line_x, 
                   draw_context.line_y, 
                   draw_context.line_num_w, 
                   draw_context.line_height,
                   tdisp->lineno.font,
                   tdisp->style);
        drawSelection(draw_context);
        drawLineText(draw_context);
        if (drawCursor(draw_context))
            drew_cursor = true;
    }
    if (drew_cursor == false) {
        drawLineno(draw_context.no, 
                   draw_context.line_x, 
                   draw_context.line_y, 
                   draw_context.line_num_w, 
                   draw_context.line_height,
                   tdisp->lineno.font,
                   tdisp->style);
        if (tdisp->focused) {
            Color color = tdisp->style->cursor.bgcolor;
            DrawRectangle(
                draw_context.line_x + draw_context.line_num_w,
                draw_context.line_y,
                3,
                draw_context.line_height,
                color);
        }
    }
    drawVerticalScrollbar(draw_context);
    freeDrawContext(&draw_context);
    EndTextureMode();

    {
        RenderTexture2D target = tdisp->texture;
        Rectangle region = elem->region;
        Rectangle src, dst;
        src.x = 0;
        src.y = 0;
        src.width  =  region.width;
        src.height = -region.height;
        dst.x = region.x;
        dst.y = region.y;
        dst.width  = region.width;
        dst.height = region.height;
        Vector2 org = {0, 0};
        DrawTexturePro(target.texture, src, 
                       dst, org, 0, WHITE);
    }
}

static void freeCallback(GUIElement *elem)
{
    TextDisplay *tdisp = (TextDisplay*) elem;
    UnloadRenderTexture(tdisp->texture);
    UnloadFont(tdisp->text.font);
    UnloadFont(tdisp->lineno.font);
    GapBuffer_free(&tdisp->buffer);
    free(elem);
}

static const GUIElementMethods methods = {
    .free = freeCallback,
    .tick = tickCallback,
    .draw = drawCallback,
    .clickUp = clickUpCallback,
    .onFocusLost = onFocusLost,
    .onFocusGained = onFocusGained,
    .onClickDown = onClickDownCallback,
    .offClickDown = offClickDownCallback,
    .onMouseWheel = onMouseWheelCallback,
    .onMouseMotion = onMouseMotionCallback,
    .onArrowLeftDown = onArrowLeftDownCallback,
    .onArrowRightDown = onArrowRightDownCallback,
    .onReturnDown = onReturnDownCallback,
    .onBackspaceDown = onBackspaceDownCallback,
    .onTextInput = onTextInputCallback,
    .onPaste = onPasteCallback,
    .onCopy = onCopyCallback,
    .onCut = onCutCallback,
    .onSave = onSaveCallback,
    .onOpen = onOpenCallback,
    .getHovered = NULL,
    .onResize = onResizeCallback,
    .openFile = openFileCallback,
    .getMinimumSize = getMinimumSize,
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
        tdisp->base.name[sizeof(tdisp->base.name)-1] = '\0';
        tdisp->old_region = region;
        tdisp->style = style;
        tdisp->v_scroll.active = false;
        tdisp->v_scroll.force = 0;
        tdisp->v_scroll.amount = 0;
        tdisp->v_scroll.start_amount = 0;
        tdisp->v_scroll.start_cursor = 0;
        tdisp->focused = false;
        tdisp->selecting = false;
        tdisp->selection.active = false;
        tdisp->texture = LoadRenderTexture(region.width, 
                                           region.height);

        if (style->text.font_data == NULL)
            tdisp->text.font = LoadFontEx(style->text.font_file, 
                                          style->text.font_size, 
                                          NULL, 250);
        else
            tdisp->text.font = LoadFontFromMemory(".ttf", style->text.font_data, 
                                                  style->text.font_data_size, 
                                                  style->text.font_size, NULL, 250);
        if (style->lineno.font_data == NULL)
            tdisp->lineno.font = LoadFontEx(style->lineno.font_file, 
                                            style->lineno.font_size, 
                                            NULL, 250);
        else
            tdisp->lineno.font = LoadFontFromMemory(".ttf", style->lineno.font_data, 
                                                    style->lineno.font_data_size, 
                                                    style->lineno.font_size, NULL, 250);

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
