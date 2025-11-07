int compare_u16(const void *a, const void *b) {
    uint16_t ua = *(const uint16_t *)a;
    uint16_t ub = *(const uint16_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// tprint: formats a string like printf and returns a malloc'ed string
char* tprint(const char* format, ...) {
    va_list args;

    // Step 1: figure out the required size
    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args) + 1; // +1 for null terminator
    va_end(args);

    // Step 2: allocate memory
    char* buffer = malloc(size);
    if (!buffer) return NULL;

    // Step 3: write formatted string into buffer
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);

    return buffer; // caller must free!
}