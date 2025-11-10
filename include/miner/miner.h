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
    FIELD_VECTOR,
    FIELD_OBJ_ARR,
    FIELD_SCALAR_ARR,
    FIELD_FLATBUFFER,
    FIELD_TABLE,
    FIELD_U64,
    FIELD_U32,
    FIELD_U32_PTR,
    FIELD_U32_STRING,
    FIELD_U16,
    FIELD_U8
} FlatbufferType;

typedef struct {
    byte* data;
    FlatbufferType* types;
    byte* name;
    int layer;

} FlatbufferTable;

typedef struct {
    byte* data;
    long  size; // this is SIZE IN BYTES. not length. Length is size / 8 (uchar)

    FlatbufferTable root_table;

} FlatbufferFile;

const char* flatbuffer_type_name(FlatbufferType t) {
    switch (t) {
        case FIELD_U8:          return "FIELD_U8";
        case FIELD_U16:         return "FIELD_U16";
        case FIELD_U32:         return "FIELD_U32";
        case FIELD_U32_STRING:  return "FIELD_U32_STRING";
        case FIELD_FLATBUFFER:  return "FIELD_FLATBUFFER";
        case FIELD_U32_PTR:     return "FIELD_U32_PTR";
        case FIELD_U64:         return "FIELD_U64";
        case FIELD_VECTOR:      return "FIELD_VECTOR";
        case FIELD_OBJ_ARR:     return "FIELD_OBJ_ARR";
        case FIELD_SCALAR_ARR:  return "FIELD_SCALAR_ARR";

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

char check_ptr (FlatbufferFile* fb_file, byte* ptr) {
    return (ptr > fb_file->data && ptr < fb_file->data + fb_file->size);
}

char is_valid_table(FlatbufferFile* fb_file, byte* table_data) {
    if (!check_ptr(fb_file, (u8*)table_data)) { return 0; }
    byte* table = table_data;
    byte* vtable_loc;
    s32 vtable_relative_offset = *(s16*)table;



    vtable_loc = table - vtable_relative_offset;

    if (!check_ptr(fb_file, (u8*)vtable_loc)) { return 0; }

    u16* vtable = (u16*)vtable_loc;

    if (!check_ptr(fb_file, (u8*)vtable)) { return 0; }
    if (!check_ptr(fb_file, (u8*)vtable+1)) { return 0; }


    u16 vsize = *(u16*)vtable;
    u16 tsize = *(u16*)(vtable+1);
    u16* vdat = vtable + 2;

    u16 element_count = (vsize - 4) / 2;

    //printf("%d / %d ", vtable_relative_offset, vsize);
    char vtable_safe = (vdat > fb_file->data && vdat < fb_file->data + fb_file->size);
    char table_safe  = (table > fb_file->data && table < fb_file->data + fb_file->size);

    for (u16 i = 0; i < element_count; i++) {
        byte* field = table + vdat[i];
        if (!check_ptr(fb_file, field)) { return 0; }
    }


    return (vsize > 0 && vtable_safe && table_safe);
}

char guesstimate_size(u16* membs, int num_membs, u16 offset) {
    if (!membs) { return -1; }

    for (int i = 0; i < num_membs; i++) {

        if (membs[i] == offset) { //offset is a valid offset
            if (i+1 < num_membs) { //ensure there is another valid offset after current
                return membs[i+1] - offset; //get distance of next member
            }
            return 0;
        }
    }

    return -1;
}

void analyze_table(FlatbufferFile* fb_file, FlatbufferTable fb_table) {
    printf("%*s", fb_table.layer*8, "");
    printf("Analyzing Table %s\n", fb_table.name);

    if (!is_valid_table(fb_file, fb_table.data)) {
        printf("BAD TABLE\n");
        return;
    }

    // setup vtable and table pointers
    byte* table = fb_table.data;
    byte* vtable_loc;
    s32 vtable_relative_offset = *(s16*)table;



    vtable_loc = table - vtable_relative_offset;
    u16* vtable = (u16*)vtable_loc;


    // first 2 bytes of vtable describe size of vtable, next 2 bytes are size of table
    // HANGING PROBLEM HERE:
    u16 vsize = *(u16*)vtable;
    u16 tsize = *(u16*)(vtable+1);
    u16* vdat = vtable + 2;

    u16 element_count = (vsize - 4) / 2;

    u16* offsets = malloc((vsize - 2));
    memcpy(offsets, vdat-1, (vsize - 2));
    qsort(offsets, element_count + 1, sizeof(u16), compare_u16);

    fb_table.types = malloc((vsize - 2) * sizeof(FlatbufferType));

    for (u16 i = 0; i < element_count; i++) {
        printf("%*s%u: ", fb_table.layer*8, "", i);

        byte* field = table + vdat[i];
        int size = guesstimate_size(offsets, element_count + 1, vdat[i]);
        FlatbufferType type = FIELD_UNKNOWN;

        char attempt_nest = 0;
        byte* nest_data = NULL;

        if (size == 0) {
            printf("default\n");
            continue;
        }
        printf("{%2u} ", size);
        switch(size) {
        case 1: type = FIELD_U8; break;
        case 2: type = FIELD_U16; break;
        case 4: {
            // 4 bytes is a special case, it could be a pointer to a table, obect, vector, etc... or just a standard int.
            type = FIELD_U32;

            //TODO Check if these are the same and cut down on redundancy for print
            printf("Possible Ints U32: %d, S32: %d ", *(u32*)field,  *(s32*)field);

            byte* vec = (field + *(u32*)field); // start of vec, including the 4 byte size header
            int vec_size = *(u32*)vec;

            // Try for string
            if (vec_size > 0 && strlen(vec+4) == vec_size) { // if cstring length is shorter than expected vec length, then it cant be a string
                printf("Possible String: \"%s\" ", vec+4);
                type = FIELD_U32_STRING;
                break;
            }

            // Generic vec
            if (vec_size > 0) {
                type = FIELD_VECTOR;
                printf("Possible Vector Size: [%u] ", vec_size);
                byte* first_offset = vec + 4;
                byte* first_table  = first_offset + *(u32*)first_offset;
                byte* last_offset  = vec + 4 + 4 * (vec_size - 1);
                byte* last_table   = last_offset + *(u32*)last_offset;


                // /printf("Could be Object[] ");
                // printf("FIRST TABLE: %X ", first_table - fb_file->data);
                // printf("LAST  TABLE: %X ", last_table - fb_file->data);

                if (is_valid_table(fb_file, first_table)) {
                    if (is_valid_table(fb_file, last_table)) {

                        type = FIELD_OBJ_ARR;
                        attempt_nest = 1;
                        nest_data = first_table;

                        break;
                    }
                    printf("THIS IS NESTED FLATBUFFER ");

                    type = FIELD_FLATBUFFER;
                    attempt_nest = 1;
                    nest_data = vec;

                }

            }
            break;
        }
        case 8: {
                type = FIELD_U64; break;
            }

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

        fb_table.types[i] = type;
        printf("Global Offset: %X ", field - fb_file->data);
        printf("Analyzed Type: %s ", flatbuffer_type_name(type));


        printf("\n");

        if (attempt_nest) {
            switch(type) {
            case FIELD_OBJ_ARR: {
                    FlatbufferTable sub = {0};
                    sub.data = nest_data;
                    sub.layer = fb_table.layer + 1;
                    sub.name = tprint("Unnamed Table %u", sub.layer);
                    analyze_table(fb_file, sub);

                    free(sub.name);
                    break;
                }
            case FIELD_FLATBUFFER: {
                    // FlatbufferFile file = {0};
                    // file.data = nest_data + 4;
                    // file.size = *(u32*) nest_data;

                    // file.root_table.data = file.data + *(u32*)file.data;
                    // file.root_table.layer = fb_table.layer + 1;
                    // file.root_table.name = "Root";

                    // analyze_table(&file, file.root_table);

                    // break;
                }
            }
        }
    }
    free(offsets);
}


