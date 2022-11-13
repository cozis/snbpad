#ifndef SNBPAD_GAPITER_H
#define SNBPAD_GAPITER_H

#include <stddef.h>
#include <stdbool.h>
#include "gap.h"

typedef struct {
    GapBuffer *buf;
    size_t cur;
    char temp[256];
} GapBufferIter;

typedef struct {
    char  *str;
    size_t off;
    size_t len;
} Line;

void GapBufferIter_init(GapBufferIter *iter, GapBuffer *buf);
void GapBufferIter_free(GapBufferIter *iter);
bool GapBufferIter_nextLine(GapBufferIter *iter, Line *line);
bool GapBufferIter_getLine(GapBufferIter *iter, size_t idx, Line *line);

#endif