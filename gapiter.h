#ifndef SNBPAD_GAPITER_H
#define SNBPAD_GAPITER_H

#include <stddef.h>
#include <stdbool.h>
#include "gap.h"

typedef struct {
    GapBuffer *buf;
    size_t cur;
    bool crossed_gap;
    char *prev_line_maybe;

    char   overw_by_zero;
    bool   placed_zero;
    size_t zero_offset;
} GapBufferIter;

typedef struct {
    char  *str;
    size_t off;
    size_t len;
} Slice;

void GapBufferIter_init(GapBufferIter *iter, GapBuffer *buf);
void GapBufferIter_free(GapBufferIter *iter);
bool GapBufferIter_nextLine(GapBufferIter *iter, Slice *line);
bool GapBufferIter_getLine(GapBufferIter *iter, size_t idx, Slice *line);

#endif