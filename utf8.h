
size_t UTF8_fromUTF32(char *dst,     size_t dstmax, uint32_t  codepoint);
size_t UTF8_toUTF32(const char *src, size_t srcmax, uint32_t *codepoint);
