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

typedef enum {
    FIELD_UNKNOWN,
    FIELD_U64,
    FIELD_U32,
    FIELD_U32_PTR,
    FIELD_U32_STRING,
    FIELD_U16,
    FIELD_U8
} FlatbufferType;

typedef struct {
    byte* data;
    long  size; // this is SIZE IN BYTES. not length. Length is size / 8 (uchar)

    byte* table;
    byte* vtable;
    
    u32* types;

} FlatbufferFile;

const char* flatbuffer_type_name(FlatbufferType t) {
    switch (t) {
        case FIELD_U8:          return "FIELD_U8";
        case FIELD_U16:         return "FIELD_U16";
        case FIELD_U32:         return "FIELD_U32";
        case FIELD_U32_STRING:  return "FIELD_U32_STRING";
        case FIELD_U32_PTR:     return "FIELD_U32_PTR";
        case FIELD_U64:         return "FIELD_U64";
        default:                return "FIELD_UNKNOWN";
    }
}

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
    // first 2 bytes of vtable describe size of vtable, next 2 bytes are size of table
    u16 vsize = (*ptr);
    u16 tsize = (*++ptr);

    // number of fields in the vtable
    u16 vlen = (vsize - 4) / 2;
    
    // copy the vtable offsets to a buffer
    // and then sort it. Makes getting sizes easier, because
    // n+1 - n = size of n
    // Slap on the size of buffer as well, so that
    // the last field can determine its size, since normally n+1 wouldnt exist.
    u16* offsets = malloc((vsize - 2));
    memcpy(offsets, ptr, (vsize - 2));
    qsort(offsets, vlen + 1, sizeof(u16), compare_u16);
    
    fb_buff->types = malloc((vsize - 2) * sizeof(FlatbufferType));

    printf("\nRoot:\n");

    int i = 0;
    while (++ptr < fb_buff->vtable + vsize) {
        if (*ptr == 0) {
            printf("%u: default\n", i++);
        } else {
            int size = guesstimate_size(offsets, vlen + 1, *ptr);
            byte* field = fb_buff->table + *ptr;
            
            FlatbufferType type = FIELD_UNKNOWN;
            
            printf("%u: <%2.u> {%d} ", i++, *ptr, size);
            
            switch(size) {
            case 1: type = FIELD_U8; break;
            case 2: type = FIELD_U16; break;
            case 4: {
                    // 4 bytes is a special case, it could be a pointer to a table, obect, vector, etc... or just a standard int.
                    type = FIELD_U32;
                    
                    //TODO Check if these are the same and cut down on redundancy for print
                    printf("Possible Ints U32: %d, S32: %d ", *(u32*)field,  *(s32*)field);
                    
                    byte* table_offset_loc = (field);
                    int table_offset = *(u16*)table_offset_loc;
                    byte* vec = (table_offset + field); // start of vec, including the 4 byte size header
                    int vec_size = *(u32*)vec;
                    
                    // Generic vec
                    if (vec_size > 0) {
                        printf("Possible Vector Size: [%d] ", vec_size);                    
                    }
                    
                    // Try for string
                    if (strlen(vec+4) == vec_size) { // if cstring length is shorter than expected vec length, then it cant be a string
                        printf("Possible String: \"%s\" ", vec+4);
                        type = FIELD_U32_STRING;
                    }
                    
                    // Try for table array
                                 
                    break; 
                }
            case 8: type = FIELD_U64; break;
            case 12: { // 12 isnt a standard flatbuffers type, but it can sometimes represent 4 floats: x y z
                    type = FIELD_UNKNOWN;
                
                    float x = *(field);
                    float y = *(field + 4);
                    float z = *(field + 8);
                    
                    printf("Possible Vec3: x: %.1f, y: %.1f, z: %.1f ", x, y, z);
      
                    break;
                }
            default: type = FIELD_UNKNOWN; break;   
            }
            
            fb_buff->types[i] = type;
            printf("Analyzed Type: %s", flatbuffer_type_name(type));
            
            printf("\n");
        }
    }

    free(offsets);
    //remember, u8 and then u32 prolly means union (specifier + tableptr)
}
