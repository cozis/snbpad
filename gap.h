#ifndef SNBPAD_GAP_H
#define SNBPAD_GAP_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *data;
    size_t size;
    size_t gap_offset;
    size_t gap_length;
    size_t lineno;
} GapBuffer;

void   GapBuffer_initEmpty(GapBuffer *buf);
bool   GapBuffer_initFile(GapBuffer *buf, const char *file);
void   GapBuffer_free(GapBuffer *buf);
size_t GapBuffer_getUsage(GapBuffer *buffer);
size_t GapBuffer_getLineno(GapBuffer *buf);
void   GapBuffer_setCursor(GapBuffer *buf, size_t cur);
bool   GapBuffer_insertString(GapBuffer *buf, const char *str, size_t len);
bool   GapBuffer_moveCursorBackward(GapBuffer *buf);
bool   GapBuffer_moveCursorForward(GapBuffer *buf);
bool   GapBuffer_removeBackwards(GapBuffer *buffer);
void   GapBuffer_removeRangeAndSetCursor(GapBuffer *buffer, size_t offset, size_t length);
char  *GapBuffer_copyRange(GapBuffer *buffer, size_t offset, size_t length);
bool   GapBuffer_saveToStream(GapBuffer *buffer, FILE *stream);
#endif