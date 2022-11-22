#include <stddef.h>
#include <raylib.h>

float renderString(Font font, const char *str, size_t len,
                   int off_x, int off_y, float font_size, 
                   Color tint);

float 
calculateStringRenderWidth(Font font, int font_size,
                           const char *str, size_t len);

size_t 
longestSubstringThatRendersInLessPixelsThan(Font font, int font_size,
                                            const char *str, size_t len, 
                                            float max_px_len);
