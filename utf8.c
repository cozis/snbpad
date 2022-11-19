#include <stddef.h>
#include <stdint.h>
#include "utf8.h"

#define MIN_UTF8_SYM_LEN 4

size_t UTF8_fromUTF32(char *dst, size_t dstmax, 
                      uint32_t codepoint)
{
    assert(dst != NULL && dstmax >= MIN_UTF8_SYM_LEN);

    size_t size;
    if (codepoint < 128) {
        dst[0] = codepoint;
        size = 1;
    } else if (codepoint < 2048) {
        dst[0] = 0xc0 | (codepoint >> 6);
        dst[1] = 0x80 | (codepoint & 0x3f);
        size = 2;
    } else if (codepoint < 65536) {
        dst[0] = 0xe0 | (codepoint >> 12);
        dst[1] = 0x80 | ((codepoint >> 6) & 0x3f);
        dst[2] = 0x80 | (codepoint & 0x3f);
        size = 3;
    } else if (codepoint <= 0x10ffff) {
        dst[0] = 0xf0 | (codepoint >> 18);
        dst[1] = 0x80 | ((codepoint >> 12) & 0x3f);
        dst[2] = 0x80 | ((codepoint >>  6) & 0x3f);
        dst[3] = 0x80 | (codepoint & 0x3f);
        size = 4;
    } else
        size = 0; // Invalid codepoint
    return size;
}

size_t UTF8_toUTF32(const char *src, size_t srcmax,
                    uint32_t *codepoint)
{
    assert(src != NULL);

    uint32_t fallback_codepoint;
    if (codepoint == NULL)
        codepoint = &fallback_codepoint;

    if (srcmax == 0)
        return 0;

    size_t size;
    if (!(src[0] & 0x80)) {
        size = 1;
        *codepoint = src[0];
    } else if (src[0] >= 0xf0) {
        if (srcmax < 4) {
            size = 0;
            *codepoint = 0;
        } else {
            size = 4;
            uint32_t temp;
            temp = (((uint32_t) src[0] & 0x07) << 18) 
                 | (((uint32_t) src[1] & 0x3f) << 12)
                 | (((uint32_t) src[2] & 0x3f) <<  6)
                 | (((uint32_t) src[3] & 0x3f));
            if (temp > 0x10ffff) {
                size = 0;
                *codepoint = 0;
            } else {
                size = 4;
                *codepoint = temp;
            }
        }
    } else if (src[0] >= 0xe0) {
        if (srcmax < 3) {
            size = 0;
            *codepoint = 0;
        } else {
            uint32_t temp;
            temp = (((uint32_t) src[0] & 0x0f) << 12)
                 | (((uint32_t) src[1] & 0x3f) <<  6)
                 | (((uint32_t) src[2] & 0x3f));
            if(temp > 0x10ffff) {
                size = 0;
                *codepoint = 0;
            } else {
                size = 3;
                *codepoint = temp;
            }
        }
    } else if (src[0] >= 0xc0) {
        if (srcmax < 2) {
            size = 0;
            *codepoint = 0;
        } else {
            size = 2;
            *codepoint = (((uint32_t) src[0] & 0x1f) << 6)
                       | (((uint32_t) src[1] & 0x3f));
            assert(*codepoint <= 0x10ffff);
        }
    } else {
        if (srcmax < 1) {
            size = 0;
            *codepoint = 0;
        } else {
            size = 1;
            *codepoint = src[0] & 0x3f;
        }
    }
    return size;
}
