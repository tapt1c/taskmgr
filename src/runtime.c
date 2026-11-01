/* Compiler-generated memory operations, without a redistributable CRT. */
#include <stddef.h>
void *memset(void *dest, int value, size_t count)
{
    unsigned char *p = (unsigned char *)dest;
    while (count--) *p++ = (unsigned char)value;
    return dest;
}
void *memcpy(void *dest, const void *source, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)source;
    while (count--) *d++ = *s++;
    return dest;
}
