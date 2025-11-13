#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

int compare_u16(const void *a, const void *b) {
    uint16_t ua = *(const uint16_t *)a;
    uint16_t ub = *(const uint16_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

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

void trunc_printf(const char *s) {
    size_t len = strlen(s);

    if (len > 16) {
        // Print first 6 chars, then "...", then last 6 chars
        printf("%.8s...%.8s", s, s + len - 8);
    } else {
        // Print string padded on the left so the end aligns to 12 chars
        printf("%*s", 16, s);
    }
}

void get_filename(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) return;

    // Find last slash or backslash
    const char *slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');

    const char *filename = slash ? slash + 1 : path;

    // Copy filename safely
    strncpy(out, filename, out_size - 1);
    out[out_size - 1] = '\0';

    // Remove extension
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}
