#ifndef XUTF8_H
#define XUTF8_H
#include <stdint.h> // uint32_t
int   xutf8_sequence_from_utf32_codepoint(char *utf8_data, int nbytes, uint32_t utf32_code);
int   xutf8_sequence_to_utf32_codepoint(const char *utf8_data, int nbytes, uint32_t *utf32_code);
int   xutf8_strlen(const char *utf8_data, int nbytes);
int   xutf8_prev(const char *utf8_data, int nbytes, int idx, uint32_t *utf32_code);
int   xutf8_next(const char *utf8_data, int nbytes, int idx, uint32_t *utf32_code);
int   xutf8_curr(const char *utf8_data, int nbytes, int idx, uint32_t *utf32_code);
#endif // #ifndef UTF8_H
