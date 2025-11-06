#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "util.c"

#define byte unsigned char

typedef uint8_t   u8;
typedef int8_t    s8;

typedef uint16_t  u16;
typedef int16_t   s16;

typedef uint32_t  u32;
typedef int32_t   s32;

typedef uint64_t  u64;
typedef int64_t   s64;

typedef struct {
    byte* data;
    long  size; // this is SIZE IN BYTES. not length. Length is size / 8 (uchar)

    byte* table;
    byte* vtable;

} FlatbufferFile;

// load flatbuffer and allocate new memory for it.
// Whole file gets loaded into RAM. This is probably bad for larger
// files but will probably be faster for smaller files. Should make this
// an option.
void alloc_flatbuffer(FlatbufferFile* fb_buff, char* path) {
    long filelen = 0;
    FILE* fileptr;

    if (!fb_buff) { return; }

    // I stole this file opening code from stack overflow i hope its fine

    fileptr = fopen(path, "rb");
    fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
    filelen = ftell(fileptr);             // Get the current byte offset in the file
    rewind(fileptr);                      // Jump back to the beginning of the file

    fb_buff->size = filelen;
    fb_buff->data = (char *)malloc(filelen * sizeof(char)); // Enough memory for the file
    fread(fb_buff->data, 1, filelen, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file
}

void set_table_pointers(FlatbufferFile* fb_buff) {
    s32 table_relptr    = fb_buff->data[0]; //First byte of flatbuffer points to the initial table
    s16 vtable_relptr   = 0;

    fb_buff->table  = fb_buff->data + table_relptr * sizeof(char); //The offset from the beginning is the value of that byte

    vtable_relptr   = fb_buff->table[0] * sizeof(char); //The offset for the vtable is described as the first byte of the table
    fb_buff->vtable = fb_buff->table - vtable_relptr; //its backwards though so gotta subtract
}

char guesstimate_size(u16* membs, int num_membs, u16 offset) {
    if (!membs) { return -1; }

    for (int i = 0; i < num_membs; i++) {

        if (membs[i] == offset) { //offset is a valid offset
            if (i+1 < num_membs) { //ensure there is another valid offset after current
                return membs[i+1] - offset; //get distance of next member
            }
            return 0; // TODO: Last offset size is calculated from tablesize - offset
        }
    }

    return -1;
}

void read_vtable(FlatbufferFile* fb_buff) {
    u16* ptr = (u16*)fb_buff->vtable;
    int size = (*ptr);

    int num_membs = (size-2) / 2;

    // copy the vtable offsets to a buffer
    // and then sort it. Makes getting sizes easier, because
    // n+1 - n = size of n
    u16* offsets = malloc(num_membs * sizeof(u16));
    memcpy(offsets, ptr+1, size-2);

    qsort(offsets, num_membs, sizeof(u16), compare_u16);

    printf("\nVtable members (%u):\n", num_membs);

    int id = 0;
    ptr = ptr+2;
    while (ptr++ < fb_buff->vtable + size) {
        if (*ptr == 0) {
            printf("%u: default\n", id++);
        } else {
            printf("%u: %u {%d}\n", id++, *ptr, guesstimate_size(offsets, num_membs, *ptr));
        }
    }

    free(offsets);
    //remember, u8 and then u32 prolly means union (specifier + tableptr)
}

