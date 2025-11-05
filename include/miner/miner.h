#include <stdlib.h>
#include <stdint.h>

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

// load flatbuffer and allocate new memory for it
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


