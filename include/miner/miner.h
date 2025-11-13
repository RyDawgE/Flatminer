#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
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
    FIELD_DEFAULT,
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
    FIELD_VEC3,
    FIELD_U16,
    FIELD_U8
} FlatbufferType;

typedef struct {
    byte* data;
    byte* name;
    int layer;

    char* possible_schema_name;

    int     num_fields; //denotes the size of the following arrs
    char**          field_names;
    FlatbufferType* field_types;
    byte**          field_data;

} FlatbufferTable;

typedef struct {
    byte* data;
    long  size; // this is SIZE IN BYTES. not length. Length is size / 8 (uchar)
    char* name;

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
        case FIELD_VEC3:        return "FIELD_VEC3";
        case FIELD_DEFAULT:     return "FIELD_DEFAULT";

        default:                return "FIELD_UNKNOWN";
    }
}

const char* flatbuffer_type_builder(FlatbufferType t) {
    switch (t) {
        case FIELD_U8:          return "bool";
        case FIELD_U16:         return "short";
        case FIELD_U32:         return "int";
        case FIELD_U32_STRING:  return "string";
        case FIELD_FLATBUFFER:  return "[ubyte] (nested_flatbuffer:\"%s\")";
        case FIELD_U32_PTR:     return "%s";
        case FIELD_U64:         return "long";
        case FIELD_VECTOR:      return "[int]"; //TODO THIS MIGHT NOT BE RIGHT
        case FIELD_OBJ_ARR:     return "[%s]";
        case FIELD_VEC3:        return "Vec3f";
        case FIELD_DEFAULT:     return "int /* Default */";

        default:                return "int /* Unknown type size: %s */";
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

void free_table(FlatbufferTable* fb_table) {
    free(fb_table->field_names);
    free(fb_table->field_types);
    free(fb_table->field_data);
}

char check_ptr (FlatbufferFile* fb_file, byte* ptr) {
    return (ptr > fb_file->data && ptr < fb_file->data + fb_file->size);
}

char is_valid_table(FlatbufferFile* fb_file, byte* table_data) {
    if (!check_ptr(fb_file, (u8*)table_data)) { return 0; }

    byte* table = table_data;
    byte* vtable_loc;
    s32 vtable_relative_offset = *(s32*)table;

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

void analyze_table(FlatbufferFile* fb_file, FlatbufferTable* fb_table) {
    printf("%*s", fb_table->layer*8, "");
    printf("Analyzing Table %s\n", fb_table->name);

    if (!is_valid_table(fb_file, fb_table->data)) {
        printf("BAD TABLE\n");
        return;
    }

    // setup vtable and table pointers
    byte* table = fb_table->data;
    byte* vtable_loc;
    s32 vtable_relative_offset = *(s32*)table;

    vtable_loc = table - vtable_relative_offset;
    u16* vtable = (u16*)vtable_loc;

    // first 2 bytes of vtable describe size of vtable, next 2 bytes are size of table
    u16 vsize = *(u16*)vtable;
    u16 tsize = *(u16*)(vtable+1);
    u16* vdat = vtable + 2;

    u16 element_count = (vsize - 4) / 2;

    u16* offsets = malloc((vsize - 2));
    memcpy(offsets, vdat-1, (vsize - 2));
    qsort(offsets, element_count + 1, sizeof(u16), compare_u16);

    fb_table->num_fields  = element_count;
    fb_table->field_names = malloc((vsize - 2) * sizeof(char*));
    fb_table->field_types = malloc((vsize - 2) * sizeof(FlatbufferType));
    fb_table->field_data  = malloc((vsize - 2) * sizeof(byte*));

    for (u16 i = 0; i < element_count; i++) {
        printf("%*s%u: ", fb_table->layer*8, "", i);

        byte* field = table + vdat[i];
        int size = guesstimate_size(offsets, element_count + 1, vdat[i]);
        FlatbufferType type = FIELD_UNKNOWN;

        char attempt_nest = 0; //for recursion
        byte* nest_data = NULL;

        if (size == 0) {
            printf("default\n");
            type = FIELD_DEFAULT;
            fb_table->field_names[i] = tprint("default_unk%u", i);
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
            printf("Possible Ints U32: %u, S32: %d ", *(u32*)field,  *(s32*)field);

            byte* vec = (field + *(u32*)field); // start of vec, including the 4 byte size header
            if (!check_ptr(fb_file, (u8*)vec)) {
                break; //If this happens, then this is not a pointer to anything. Wow!
            }

            //Try for regular object ptr
            if (is_valid_table(fb_file, vec)) {
                type = FIELD_U32_PTR;
                attempt_nest = 1;
                nest_data = vec;
            }

            // Try for string
            int vec_size = *(u32*)vec;
            if (vec_size > 0 && strlen(vec+4) == vec_size) { // if cstring length is shorter than expected vec length, then it cant be a string
                // @TODO: should be an arg for displaying full strings. Or maybe one for truncing strings? idfk.
                printf("Possible String: \"");
                trunc_printf(vec+4);
                printf("\" ");
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
                    //printf("THIS IS NESTED FLATBUFFER ");

                    type = FIELD_FLATBUFFER;
                    attempt_nest = 1;
                    nest_data = vec;
                    break;
                }
            }

            break;
        }
        case 8: {
                type = FIELD_U64; break;
        }

        case 12: { // 12 isnt a standard flatbuffers type, but it can sometimes represent 4 floats: x y z
                type = FIELD_VEC3;

                float* f = (float*)field;

                float x = *f;
                float y = *++f;
                float z = *++f;

                printf("Possible Vec3: x: %.1f, y: %.1f, z: %.1f ", x, y, z);

                fb_table->field_names[i]   = "pos";
                break;
        }

        default: { type = FIELD_UNKNOWN; } break;
        }

        fb_table->field_types[i] = type;
        fb_table->field_data[i]  = field;
        fb_table->field_names[i] = tprint("unk%u", i);

        printf("Global Offset: %X ", field - fb_file->data);
        printf("Analyzed Type: %s ", flatbuffer_type_name(type));
        printf("\n");

        if (attempt_nest) {
            switch(type) {
                case FIELD_OBJ_ARR: {
                        FlatbufferTable sub = {0};
                        sub.data = nest_data;
                        sub.layer = fb_table->layer + 1;
                        sub.name = tprint("Unnamed_%u_at_Index_0", sub.layer);
                        analyze_table(fb_file, &sub);

                        free(sub.name);
                        free_table(&sub);
                        break;
                }
                case FIELD_FLATBUFFER: {
                        FlatbufferFile file = {0};
                        file.data = nest_data + 4;
                        file.size = *(u32*) nest_data;

                        file.root_table.data = file.data + *(u32*)file.data;
                        file.root_table.layer = fb_table->layer + 1;
                        file.root_table.name = "Root";

                        if (i-1 >= 0) {
                            FlatbufferType prev_type = fb_table->field_types[i-1];
                            if (prev_type == FIELD_U32_STRING) {
                                char* field_str = fb_table->field_data[i-1] + *(u32*)(fb_table->field_data[i-1]);
                                file.root_table.name = field_str+4;

                                fb_table->field_names[i-1] = "type_name";
                                fb_table->field_names[i]   = "nested_type";
                            }
                        }

                        analyze_table(&file, &file.root_table);


                        free_table(&file.root_table);

                        break;
                }
                case FIELD_U32_PTR: {
                        FlatbufferTable sub = {0};
                        sub.data = nest_data;
                        sub.layer = fb_table->layer + 1;
                        sub.name = tprint("Unnamed_%u", sub.layer);
                        analyze_table(fb_file, &sub);

                        free(sub.name);
                        break;
                }
            }
        }
    }
    free(offsets);
}


void generate_schema(FlatbufferFile* fb_file, FlatbufferTable* fb_table) {
    if (!fb_file || !fb_table) return;
    if (!is_valid_table(fb_file, fb_table->data)) return;

    // Open file fresh each time (overwrite)
    mkdir("schemas");
    FILE *file = fopen(tprint(".\\schemas\\%s.fbs", fb_table->name), "w");
    if (!file) {
        perror("Error creating file");
        return;
    }

    // Collect includes in memory first
    char includes[4096] = {0};
    strcat(includes, "include \"common.fbs\";\n");

    fprintf(file, "table %s {\n", fb_table->name);

    for (int i = 0; i < fb_table->num_fields; i++) {
        FlatbufferType type = fb_table->field_types[i];
        char* field         = fb_table->field_data[i];
        char* name          = fb_table->field_names[i];

        char* table_name_str = tprint("%s_%u", fb_table->name, i);

        // Check for named fb types. If the previous field was a string, it probably
        // Denotes the schema name.
        if (i-1 >= 0) {
            FlatbufferType prev_type = fb_table->field_types[i-1];
            if (prev_type == FIELD_U32_STRING && type == FIELD_FLATBUFFER) {

                table_name_str = (fb_table->field_data[i-1] + *(u32*)(fb_table->field_data[i-1])) + 4;

                // If table name is hashed it might start with a number,
                // which flatbuffer doesnt like, so insert a _
                if (isdigit(table_name_str[0])) {
                    table_name_str = tprint("_%s", table_name_str);
                }

            }
        }

        if (type == FIELD_UNKNOWN) {
            table_name_str = "69420 pls implement";
        }

        //
        fprintf(file, "  %s: %s;\n",
                name,
                tprint(flatbuffer_type_builder(type), table_name_str));

        if (type == FIELD_FLATBUFFER) {
            byte* vec = field + *(u32*)field;

            FlatbufferFile file = {0};
            file.data = vec + 4;
            file.size = *(u32*)vec;

            file.root_table.data = file.data + *(u32*)file.data;
            file.root_table.layer = fb_table->layer + 1;
            file.root_table.name = table_name_str;

            analyze_table(&file, &file.root_table);
            generate_schema(fb_file, &file.root_table);


            // Save include line to prepend later
            strcat(includes, tprint("include \"%s.fbs\";\n", table_name_str));
        }

        if (type == FIELD_OBJ_ARR) {
            byte* vec = field + *(u32*)field;
            byte* first_offset = vec + 4;
            byte* first_table  = first_offset + *(u32*)first_offset;

            FlatbufferTable sub = {0};
            sub.data = first_table;
            sub.layer = 0;
            sub.name = table_name_str;

            analyze_table(fb_file, &sub);
            generate_schema(fb_file, &sub);

            // Save include line to prepend later
            strcat(includes, tprint("include \"%s.fbs\";\n", table_name_str));
        }

        if (type == FIELD_U32_PTR) {
            FlatbufferTable sub = {0};
            sub.data = field + *(u32*)field;
            sub.layer = 0;
            sub.name = table_name_str;

            analyze_table(fb_file, &sub);
            generate_schema(fb_file, &sub);

            // Save include line to prepend later
            strcat(includes, tprint("include \"%s.fbs\";\n", table_name_str));

            free_table(&sub);
        }
    }

    fprintf(file, "}\nroot_type %s;\n", fb_table->name);
    fclose(file);

    // Reopen and prepend includes cleanly
    if (strlen(includes) > 0) {
        FILE *original = fopen(tprint("%s.fbs", fb_table->name), "r");
        FILE *temp = fopen("temp.fbs", "w");

        fprintf(temp, "%s\n", includes);
        char buffer[1024];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), original)) > 0)
            fwrite(buffer, 1, n, temp);

        fclose(original);
        fclose(temp);

        remove(tprint("%s.fbs", fb_table->name));
        rename("temp.fbs", tprint("%s.fbs", fb_table->name));
    }

    printf("File %s created successfully.\n", fb_table->name);
}