#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "gap.h"
#include "utils.h"
#include "xutf8.h"

static size_t countLines(const char *str, size_t len)
{
    size_t lineno = 0;
    for (size_t i = 0; i < len; i++)
        if (str[i] == '\n')
            lineno++;
    return lineno;
}

static size_t countLinesInBuffer(GapBuffer *buf)
{
    size_t p = (buf->gap_offset + buf->gap_length);
    return countLines(buf->data, buf->gap_offset)
         + countLines(buf->data + p, buf->size - p);
}

static void moveBytesAfterGap(GapBuffer *buffer, size_t num)
{
    if (num > buffer->gap_offset)
        num = buffer->gap_offset;

    memmove(buffer->data + buffer->gap_offset + buffer->gap_length - num,
            buffer->data + buffer->gap_offset - num,
            num);

    buffer->gap_offset -= num;
}

static void moveBytesBeforeGap(GapBuffer *buffer, size_t num)
{
    size_t after_gap = buffer->size 
                     - buffer->gap_offset 
                     - buffer->gap_length;
    if (num > after_gap)
        num = after_gap;

    memmove(buffer->data + buffer->gap_offset,
            buffer->data + buffer->gap_offset + buffer->gap_length,
            num);

    buffer->gap_offset += num;
}

void GapBuffer_removeRangeAndSetCursor(GapBuffer *buffer, 
                                       size_t offset, 
                                       size_t length)
{
    if (offset + length <= buffer->gap_offset)
        moveBytesAfterGap(buffer, buffer->gap_offset - (offset + length));
    else if (offset >= buffer->gap_offset + buffer->gap_length)
        moveBytesBeforeGap(buffer, (offset + length) - buffer->gap_offset);
    buffer->lineno -= countLines(buffer->data + offset, length);
    buffer->gap_offset = offset;
    buffer->gap_length += length;
}

size_t GapBuffer_getUsage(GapBuffer *buffer)
{
    return buffer->size - buffer->gap_length;
}

size_t GapBuffer_getLineno(GapBuffer *buf)
{
    return buf->lineno;
}

bool GapBuffer_removeBackwards(GapBuffer *buffer)
{
    if (buffer->gap_offset == 0)
        return false;

    int prev = xutf8_prev(buffer->data, buffer->size, buffer->gap_offset, NULL);
    assert(prev >= 0 && (size_t) prev < buffer->gap_offset);

    buffer->gap_length += buffer->gap_offset - prev;
    buffer->gap_offset = prev;
    return true;
}

bool GapBuffer_moveCursorForward(GapBuffer *buf)
{
    if (buf->gap_offset + buf->gap_length == buf->size)
        return false;
    
    int n = xutf8_curr(buf->data, buf->size, buf->gap_offset + buf->gap_length, NULL);
    assert(n >= 0);

    if (buf->gap_offset + buf->gap_length + n == buf->size)
        moveBytesBeforeGap(buf, n);
    else {
        int next = xutf8_next(buf->data, buf->size, buf->gap_offset + buf->gap_length, NULL);
        assert(next >= 0 && (size_t) next > buf->gap_offset + buf->gap_length);

        moveBytesBeforeGap(buf, next - buf->gap_offset - buf->gap_length);
    }
    return true;
}

bool GapBuffer_moveCursorBackward(GapBuffer *buf)
{
    if (buf->gap_offset == 0)
        return false;

    int prev = xutf8_prev(buf->data, buf->size, buf->gap_offset, NULL);
    assert(prev >= 0 && (size_t) prev < buf->gap_offset);

    moveBytesAfterGap(buf, buf->gap_offset - prev);
    return true;
}

void GapBuffer_setCursor(GapBuffer *buf, size_t cur)
{
    size_t usage = GapBuffer_getUsage(buf);
    cur = MIN(cur, usage);

    if (cur < buf->gap_offset)
        moveBytesAfterGap(buf, buf->gap_offset - cur);
    else
        moveBytesBeforeGap(buf, cur - buf->gap_offset);
}

static bool growGap(GapBuffer *buf, size_t min)
{
    if (buf->data == NULL) {
        
        size_t size = 4096;
        if (size < min) size = min;

        char *data = malloc(size);
        if (data == NULL)
            return false;

        buf->data = data;
        buf->size = size;
        buf->gap_offset = 0;
        buf->gap_length = size;

    } else {
        size_t new_size = 2 * buf->size;
        char  *new_data = malloc(new_size);
        if (new_data == NULL)
            return false;

        size_t prev_used = buf->size - buf->gap_length;

        size_t tail_size = buf->size 
                         - buf->gap_offset 
                         - buf->gap_length;
        memcpy(new_data, buf->data, buf->gap_offset);
        memcpy(new_data + new_size - tail_size, buf->data + buf->size - tail_size, tail_size);
        free(buf->data);
        buf->data = new_data;
        buf->size = new_size;
        buf->gap_length = new_size - prev_used;
    }
    return true;
}

void GapBuffer_initEmpty(GapBuffer *buf)
{
    buf->data = NULL;
    buf->size = 0;
    buf->gap_offset = 0;
    buf->gap_length = 0;
    buf->lineno = 0;
}

bool GapBuffer_initFile(GapBuffer *buf, const char *file)
{
    GapBuffer_initEmpty(buf);
    if (file != NULL) {
        FILE *stream = fopen(file, "rb");
        if (stream == NULL)
            return false;

        fseek(stream, 0, SEEK_END); // TODO: Check for errors
        long size = ftell(stream); // TODO: Check for errors

        if (!growGap(buf, size)) {
            fclose(stream);
            return false;
        }

        fseek(stream, 0, SEEK_SET); // TODO: Check for errors
        fread(buf->data + buf->gap_offset + buf->gap_length - size, 1, size, stream);
        if (ferror(stream)) {
            fclose(stream);
            return false;
        }

        buf->gap_length -= size;
        buf->lineno = countLinesInBuffer(buf);
        fclose(stream);
    }
    return true;
}

void GapBuffer_free(GapBuffer *buf)
{
    free(buf->data);
}

bool GapBuffer_insertString(GapBuffer *buf, 
                            const char *str, 
                            size_t len)
{
    if (buf->gap_length < len)
        if (!growGap(buf, len))
            return false;

    memcpy(buf->data + buf->gap_offset, str, len);
    buf->gap_offset += len;
    buf->gap_length -= len;
    buf->lineno += countLines(str, len);
    return true;
}

char *GapBuffer_copyRange(GapBuffer *buffer, 
                          size_t offset, 
                          size_t length)
{
    size_t usage = GapBuffer_getUsage(buffer);
    if (offset > usage)
        offset = usage;
    if (offset + length > usage)
        length = usage - offset;

    char *dst = malloc(length+1);
    if (dst == NULL)
        return NULL;

    size_t first_copy = MIN(offset + length, buffer->gap_offset) - offset;
    memcpy(dst, buffer->data + offset, first_copy);

    if (first_copy < length)
        memcpy(dst + first_copy, buffer->data + buffer->gap_offset + buffer->gap_length, length - first_copy);

    dst[length] = '\0';
    return dst;
}

bool GapBuffer_saveToStream(GapBuffer *buffer, FILE *stream)
{
    size_t p = buffer->gap_offset 
             + buffer->gap_length;
    size_t n;
    n = fwrite(buffer->data, sizeof(char), buffer->gap_offset, stream);
    if (n < buffer->gap_offset) return false;
    n = fwrite(buffer->data + p, sizeof(char), buffer->size - p, stream);
    if (n < buffer->size - p) return false;
    return true;
}