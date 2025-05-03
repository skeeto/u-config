__attribute((section(".text.memcpy")))
void *memcpy(void *restrict dst, void *restrict src, size_t n)
{
    u8 *s = src;
    u8 *d = dst;
    for (; n; n--) *d++ = *s++;
    return dst;
}

__attribute((section(".text.memset")))
void *memset(void *restrict dst, int c, size_t n)
{
    u8 *d = dst;
    for (; n; n--) *d++ = (u8)c;
    return dst;
}
