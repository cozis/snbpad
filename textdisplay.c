#include <assert.h>
#include "utils.h"
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
        unsigned int h1 = TTF_FontHeight(tdisp->text.font);
        unsigned int h2 = TTF_FontHeight(tdisp->lineno.font);
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
        TTF_SizeUTF8(tdisp->lineno.font, max_lineno_as_text, &width, NULL);
    } else
        width = tdisp->lineno.width;
    return width;
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

    int line_w;
    TTF_SizeUTF8(tdisp->text.font, line.str, &line_w, NULL);
    
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
                int res = TTF_SizeUTF8(tdisp->text.font, line.str, &mx, NULL);
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
    const int max_scroll = TextDisplay_getLogicalHeight(tdisp) - tdisp->h;
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
        const unsigned int 
        logical_text_height = TextDisplay_getLogicalHeight(tdisp);
        int amount = tdisp->v_scroll.start_amount + logical_text_height * (double) (y - tdisp->v_scroll.start_cursor) / tdisp->h;
        setVerticalScroll(tdisp, amount);
    } else if (tdisp->selecting) {
        size_t pos = cursorFromClick(tdisp, x, y);
        tdisp->selection.end = pos;
    }
}

static void getVerticalScrollbarPosition(TextDisplay *tdisp,
                                         SDL_Rect *scrollbar, 
                                         SDL_Rect *thumb)
{
    scrollbar->w = tdisp->scrollbar_size;
    scrollbar->h = tdisp->h;
    scrollbar->x = tdisp->x + tdisp->w - scrollbar->w;
    scrollbar->y = tdisp->y;

    int logical_text_height = TextDisplay_getLogicalHeight(tdisp);
    int thumb_y = tdisp->h * (double) tdisp->v_scroll.amount / logical_text_height;
    int thumb_height = scrollbar->h * (double) tdisp->h / logical_text_height;

    thumb->x = scrollbar->x;
    thumb->y = scrollbar->y + thumb_y;
    thumb->w = scrollbar->w;
    thumb->h = thumb_height;
}

void TextDisplay_onClickDown(TextDisplay *tdisp, int x, int y)
{
    assert(x >= tdisp->x && x < tdisp->x + tdisp->w);
    assert(y >= tdisp->y && y < tdisp->y + tdisp->h);

    SDL_Rect v_scrollbar, v_thumb;
    getVerticalScrollbarPosition(tdisp, &v_scrollbar, &v_thumb);

    SDL_Rect cursor_rect;
    cursor_rect.x = x;
    cursor_rect.y = y;
    cursor_rect.w = 1;
    cursor_rect.h = 1;
    if (SDL_HasIntersection(&cursor_rect, &v_scrollbar)) {
        if (SDL_HasIntersection(&cursor_rect, &v_thumb)) {
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

void TextDisplay_onArrowDown(TextDisplay *tdisp, SDL_Keycode code)
{
    tdisp->selection.active = false;
    switch (code) {
        case SDLK_LEFT:  GapBuffer_moveCursorBackward(tdisp->buffer); break;
        case SDLK_RIGHT: GapBuffer_moveCursorForward(tdisp->buffer);  break;
    }
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

static void cutOrCopy(TextDisplay *tdisp, bool cut)
{
    if (tdisp->selection.active) {
        size_t offset, length;
        Selection_getSlice(tdisp->selection, &offset, &length);
        char *s = GapBuffer_copyRange(tdisp->buffer, offset, length);
        if (s == NULL)
            fprintf(stderr, "WARNING :: Failed to copy text from the buffer\n");
        else {
            if (SDL_SetClipboardText(s))
                fprintf(stderr, "WARNING :: Failed to copy text into the clipboard\n");
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
    if (SDL_HasClipboardText()) {
        char *s = SDL_GetClipboardText();
        if (s != NULL)
            GapBuffer_insertString(tdisp->buffer, s, strlen(s));
        SDL_free(s);
    }
}

void TextDisplay_draw(TextDisplay *tdisp, 
                      SDL_Renderer *ren)
{
    SDL_Rect text_display_rect;
    text_display_rect.x = tdisp->x;
    text_display_rect.y = tdisp->y;
    text_display_rect.w = tdisp->w;
    text_display_rect.h = tdisp->h;
    SDL_RenderSetViewport(ren, &text_display_rect);
    
    SDL_SetRenderDrawColor(ren, 
        0xee, 0xee, 0xcc, 0xff);
    SDL_RenderFillRect(ren, NULL);

    unsigned int line_height = TextDisplay_getLineHeight(tdisp);

    int y_relative = -tdisp->v_scroll.amount;
    size_t no = 0;
    GapBufferIter iter;
    GapBufferIter_init(&iter, tdisp->buffer);

    while (y_relative + (int) line_height < 0 && GapBufferIter_nextLine(&iter, NULL)) {
        y_relative += line_height;
        no++;
    }

    Slice line;
    while (y_relative < tdisp->h && GapBufferIter_nextLine(&iter, &line)) {

        SDL_Surface *surface;
        if (line.len > 0) {
            surface = TTF_RenderUTF8_Blended(tdisp->text.font, line.str, tdisp->text.fgcolor);
            if (surface == NULL)
                fprintf(stderr, "WARNING :: Failed to create line surface for \"%s\" (%ld bytes)\n", line.str, line.len);
        } else
            surface = NULL;
        
        SDL_Rect lineno_rect;
        if (tdisp->lineno.hide == false) {

            assert(tdisp->lineno.font != NULL);

            int width = TextDisplay_getLinenoColumnWidth(tdisp);

            lineno_rect.x = 0;
            lineno_rect.y = y_relative;
            lineno_rect.w = width;
            lineno_rect.h = line_height;
            if (tdisp->lineno.nobg == false) {
                SDL_SetRenderDrawColor(ren, 
                    tdisp->lineno.bgcolor.r, 
                    tdisp->lineno.bgcolor.g, 
                    tdisp->lineno.bgcolor.b, 
                    tdisp->lineno.bgcolor.a);
                SDL_RenderFillRect(ren, &lineno_rect);
            }

            char lineno_text[8];
            snprintf(lineno_text, sizeof(lineno_text), "%ld", no);
            SDL_Surface *surface = TTF_RenderUTF8_Blended(tdisp->lineno.font, lineno_text, tdisp->lineno.fgcolor);
            if (surface != NULL) {
                
                SDL_Rect lineno_text_rect;
                lineno_text_rect.w = surface->w;
                lineno_text_rect.h = surface->h;
                switch (tdisp->lineno.h_align) {
                    case TextAlignH_LEFT:   lineno_text_rect.x = lineno_rect.x; break;
                    case TextAlignH_RIGHT:  lineno_text_rect.x = lineno_rect.x + lineno_rect.w - lineno_text_rect.w; break;
                    case TextAlignH_CENTER: lineno_text_rect.x = lineno_rect.x + (lineno_rect.w - lineno_text_rect.w) / 2; break;
                }
                switch (tdisp->lineno.v_align) {
                    case TextAlignV_TOP:    lineno_text_rect.y = lineno_rect.y; break;
                    case TextAlignV_CENTER: lineno_text_rect.y = lineno_rect.y + lineno_rect.h - lineno_text_rect.h; break;
                    case TextAlignV_BOTTOM: lineno_text_rect.y = lineno_rect.y + (lineno_rect.h - lineno_text_rect.h) / 2; break;
                }

                SDL_Texture *texture = SDL_CreateTextureFromSurface(ren, surface);
                if (texture != NULL) {
                    SDL_RenderCopy(ren, texture, NULL, &lineno_text_rect);
                    SDL_DestroyTexture(texture);
                } else
                    fprintf(stderr, "WARNING :: Failed to create line texture\n");
                SDL_FreeSurface(surface);
            } else
                fprintf(stderr, "WARNING :: Failed to build lineno surface\n");
        } else {
            lineno_rect.x = 0;
            lineno_rect.y = y_relative;
            lineno_rect.w = 0;
            lineno_rect.h = line_height;
        }

        SDL_Rect line_rect;
        line_rect.x = lineno_rect.x + lineno_rect.w;
        line_rect.y = lineno_rect.y;
        if (surface == NULL) {
            line_rect.w = 0;
            line_rect.h = line_height;
        } else {
            line_rect.w = surface->w;
            line_rect.h = surface->h;
        }

        if (tdisp->selection.active) {
            
            SDL_Rect selection_rect;
            selection_rect.y = line_rect.y;
            selection_rect.h = line_rect.h;

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
                selection_rect.x = line_rect.x;
                selection_rect.w = line_rect.w;
                something_selected_in_line = true;

            } else if (line.off <= selection_head && selection_tail < line.off + line.len) {
                
                /* Selection is fully contained by this line */
                size_t relative_selection_start = selection_head - line.off;
                size_t relative_selection_end   = selection_tail   - line.off;

                int w1, w2;
                char c;
                c = line.str[relative_selection_start];
                line.str[relative_selection_start] = '\0';
                TTF_SizeUTF8(tdisp->text.font, line.str, &w1, NULL);
                line.str[relative_selection_start] = c;

                c = line.str[relative_selection_end];
                line.str[relative_selection_end] = '\0';
                TTF_SizeUTF8(tdisp->text.font, line.str, &w2, NULL);
                line.str[relative_selection_end] = c;

                selection_rect.x = line_rect.x + w1;
                selection_rect.w = w2 - w1;
                something_selected_in_line = true;

            } else if (line.off < selection_head && line.off + line.len <= selection_tail) {
                
                /* Selection goes from the middle of the line 'till the end */
                
                size_t relative_selection_start = selection_head - line.off;

                int w;
                char c;
                c = line.str[relative_selection_start];
                line.str[relative_selection_start] = '\0';
                TTF_SizeUTF8(tdisp->text.font, line.str, &w, NULL);
                line.str[relative_selection_start] = c;

                selection_rect.x = line_rect.x + w;
                selection_rect.w = line_rect.w - w;
                something_selected_in_line = true;

            } else if (selection_head < line.off && selection_tail < line.off + line.len) {

                /* Selection goes from the start to the middle */
                
                size_t relative_selection_end = selection_tail - line.off;

                int w;
                char c;
                c = line.str[relative_selection_end];
                line.str[relative_selection_end] = '\0';
                TTF_SizeUTF8(tdisp->text.font, line.str, &w, NULL);
                line.str[relative_selection_end] = c;

                selection_rect.x = line_rect.x;
                selection_rect.w = w;
                something_selected_in_line = true;

            } else {
                //assert(0); // All of the previous conditions
                           // must be mutually exclusive.
                something_selected_in_line = false; // Temporary
            }

            if (something_selected_in_line) {
                SDL_SetRenderDrawColor(ren, 
                    tdisp->text.selection_bgcolor.r, 
                    tdisp->text.selection_bgcolor.g, 
                    tdisp->text.selection_bgcolor.b, 
                    tdisp->text.selection_bgcolor.a);
                SDL_RenderFillRect(ren, &selection_rect);
            }
        
        } else {

            const size_t cursor = tdisp->buffer->gap_offset;
            if (line.off <= cursor && cursor <= line.off + line.len) {
            
                // The cursor is in this line.
                int relative_cursor_x;

                size_t m = cursor - line.off;
                char c = line.str[m];
                line.str[m] = '\0';
                int res = TTF_SizeUTF8(tdisp->text.font, line.str, 
                                       &relative_cursor_x, NULL);
                line.str[m] = c;
                
                SDL_Rect cursor_rect;
                cursor_rect.x = line_rect.x + relative_cursor_x;
                cursor_rect.y = line_rect.y;
                cursor_rect.w = 3;
                cursor_rect.h = line_height;
                SDL_SetRenderDrawColor(ren, 
                    tdisp->cursor.bgcolor.r, 
                    tdisp->cursor.bgcolor.g, 
                    tdisp->cursor.bgcolor.b, 
                    tdisp->cursor.bgcolor.a);
                SDL_RenderFillRect(ren, &cursor_rect);
            }
        }

        if (surface != NULL) {
            SDL_Texture *texture = SDL_CreateTextureFromSurface(ren, surface);
            if (texture != NULL) {
                SDL_RenderCopy(ren, texture, NULL, &line_rect);
                SDL_DestroyTexture(texture);
            } else
                fprintf(stderr, "WARNING :: Failed to create line texture\n");
            SDL_FreeSurface(surface);
        }

        y_relative += line_height;
        no++;
    }
    GapBufferIter_free(&iter);

    SDL_SetRenderDrawColor(ren, 
        0xff, 0x00, 0x00, 0xff);
    SDL_RenderDrawRect(ren, NULL);

    // Vertical scrollbar
    {
        SDL_Rect abs_scrollbar, abs_thumb;
        SDL_Rect rel_scrollbar, rel_thumb;
        getVerticalScrollbarPosition(tdisp, &abs_scrollbar, &abs_thumb);

        rel_scrollbar = abs_scrollbar;
        rel_scrollbar.x -= tdisp->x;
        rel_scrollbar.y -= tdisp->y;
        rel_thumb = abs_thumb;
        rel_thumb.x -= tdisp->x;
        rel_thumb.y -= tdisp->y;
        SDL_RenderFillRect(ren, &rel_thumb);
    }

    SDL_RenderSetViewport(ren, NULL);
}