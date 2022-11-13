#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utils.h"
#include "gapiter.h"

void GapBufferIter_init(GapBufferIter *iter, 
                        GapBuffer *buf)
{
    iter->buf = buf;
    iter->cur = 0;
}

void GapBufferIter_free(GapBufferIter *iter)
{
    (void) iter;
}

bool GapBufferIter_nextLine(GapBufferIter *iter, 
                            Line *line)
{
    Line line_fallback;
    if (line == NULL)
        line = &line_fallback;

    char  *s = iter->buf->data;
    size_t n = iter->buf->size;
    size_t goff = iter->buf->gap_offset;
    size_t glen = iter->buf->gap_length;
    size_t c = iter->cur;

    if (c == n)
        return false;

    if (c > goff) {

        size_t start = c; // Offset of the line
        while (c < n && s[c] != '\n') 
            c++;

        line->str = s + start;
        line->off = start - glen;
        line->len = c - start;

        if (c < n) 
            c++; // Skip the \n

    } else {

        size_t first_start = c; // Offset of the line
        while (c < goff && s[c] != '\n') 
            c++;
        size_t first_len = c - first_start;

        if (c < goff) {
            line->str = s + first_start;
            line->off = first_start;
            line->len = first_len;
            c++; // Consume the \n
            if (c == goff) 
                c += glen;

        } else {

            assert(c == goff);
            
            c += glen; // Skip the gap

            size_t second_start = c;
            while (c < n && s[c] != '\n') 
                c++;
            size_t second_len = c - second_start;
            
            if (c < n) 
                c++; // Consume the \n

            char  *dst = iter->temp;
            size_t max = sizeof(iter->temp);
            
            size_t  first_copy_len = MIN( first_len, max);
            size_t second_copy_len = MIN(second_len, max - first_copy_len);
            memcpy(dst,                  s +  first_start,  first_copy_len);
            memcpy(dst + first_copy_len, s + second_start, second_copy_len);
            line->str = iter->temp;
            line->off = first_start;
            line->len = first_copy_len + second_copy_len;
        }
    }
    
    iter->cur = c;
    return true;
}

bool GapBufferIter_getLine(GapBufferIter *iter, size_t idx, Line *line)
{
    for (size_t i = 0; i < idx+1; i++)
        if (!GapBufferIter_nextLine(iter, line))
            return false;
    return true;
}