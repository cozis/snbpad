#include <stdlib.h>
#include <string.h>
#include "gapiter.h"

void GapBufferIter_init(GapBufferIter *iter, GapBuffer *buf)
{
    iter->buf = buf;
    iter->cur = 0;
    iter->crossed_gap = false;
    iter->placed_zero = false;
    iter->prev_line_maybe = NULL;
}

static void undoInternalOperations(GapBufferIter *iter)
{
    if (iter->placed_zero) {
        iter->buf->data[iter->zero_offset] = iter->overw_by_zero;
        iter->placed_zero = false;
    }

    if (iter->prev_line_maybe != NULL) {
        free(iter->prev_line_maybe);
        iter->prev_line_maybe = NULL;
    }
}

void GapBufferIter_free(GapBufferIter *iter)
{
    undoInternalOperations(iter);
}

bool GapBufferIter_nextLine(GapBufferIter *iter, Slice *line)
{
    undoInternalOperations(iter);

    if (iter->cur == iter->buf->size)
        return false;

    if (iter->crossed_gap) {

        size_t line_offset = iter->cur; // Offset of the line
        while (iter->cur < iter->buf->size && iter->buf->data[iter->cur] != '\n')
            iter->cur++;
        size_t line_length = iter->cur - line_offset;

        if (iter->cur == iter->buf->size) {

            if (line != NULL) {
                char *s = malloc(line_length+1);
                if (s == NULL) {
                    line->str = "";
                    line->off = line_offset - iter->buf->gap_length;
                    line->len = 0;
                } else {
                    memcpy(s, iter->buf->data + line_offset, line_length);
                    s[line_length] = '\0';
                    line->str = s;
                    line->off = line_offset - iter->buf->gap_length;
                    line->len = line_length;
                    iter->prev_line_maybe = s;
                }
            }
        } else {

            if (line != NULL) {

                line->str = iter->buf->data + line_offset;
                line->off = line_offset - iter->buf->gap_length;
                line->len = line_length;

                // Overwrite \n with \0
                iter->overw_by_zero = iter->buf->data[iter->cur];
                iter->placed_zero = true;
                iter->zero_offset = iter->cur;
                iter->buf->data[iter->cur] = '\0';
            }

            iter->cur++; // Skip the \n
        }

    } else {

        size_t first_half_offset = iter->cur; // Offset of the line
        while (iter->cur < iter->buf->gap_offset && iter->buf->data[iter->cur] != '\n')
            iter->cur++;
        size_t first_half_length = iter->cur - first_half_offset;

        if (iter->cur == iter->buf->gap_offset) {

            iter->cur += iter->buf->gap_length; // Skip the gap
            iter->crossed_gap = true;

            size_t second_half_offset = iter->cur;
            while (iter->cur < iter->buf->size && iter->buf->data[iter->cur] != '\n')
                iter->cur++;
            size_t second_half_length = iter->cur - second_half_offset;

            if (iter->cur < iter->buf->size)
                iter->cur++; // Consume the \n

            if (line != NULL) {
                char *s = malloc(first_half_length + second_half_length + 1);
                if (s == NULL) {
                    line->str = "";
                    line->off = first_half_offset;
                    line->len = 0;
                } else {
                    memcpy(s, iter->buf->data + first_half_offset, first_half_length);
                    memcpy(s + first_half_length, iter->buf->data + second_half_offset, second_half_length);
                    s[first_half_length + second_half_length] = '\0';
                    line->str = s;
                    line->off = first_half_offset;
                    line->len = first_half_length + second_half_length;
                    iter->prev_line_maybe = s;
                }
            }

        } else {

            if (line != NULL) {

                size_t line_offset = first_half_offset;
                size_t line_length = first_half_length;

                line->str = iter->buf->data + line_offset;
                line->off = line_offset;
                line->len = line_length;

                // Overwrite \n with \0
                iter->overw_by_zero = iter->buf->data[iter->cur];
                iter->placed_zero = true;
                iter->zero_offset = iter->cur;
                iter->buf->data[iter->cur] = '\0';
            }

            iter->cur++; // Consume the \n
        }
    }

    return true;
}

bool GapBufferIter_getLine(GapBufferIter *iter, size_t idx, Slice *line)
{
    for (size_t i = 0; i < idx; i++)
        if (!GapBufferIter_nextLine(iter, NULL))
            return false;
    if (!GapBufferIter_nextLine(iter, line))
        return false;
    return true;
}
