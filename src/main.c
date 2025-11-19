#include "stdio.h"
#include "miner\miner.h"


int main(int argc, char *argv[]) {
    char* path = "..\\data\\a.bin";
    char generate_schemas = 1;

    if (argc < 2) {
        printf("ERROR: Please provide a valid filepath\n");
        return -1;
    } else {
        path = argv[1];
    }

    for (int i = 0; i < argc; i++) {
        char* arg = argv[i];

        if (strcmp(arg, "-ao")) {
            generate_schemas = 0;
        }

    }

    printf("Hello, Flatminer!\n");
    printf("Mining |%s|\n", path);

    FlatbufferFile* file = malloc(sizeof(FlatbufferFile));
    alloc_flatbuffer(file, path);
    file->root_table.data = file->data + *(u32*)file->data;
    file->root_table.layer = 0;

    char table_name[260];
    get_filename(path, table_name, sizeof(table_name));

    file->root_table.name = table_name;

    analyze_table(file, &file->root_table);

    FlatbufferTable* tbl = &file->root_table;

    if (generate_schemas == 1) {
        generate_schema(file, tbl);
    }

    free_table(tbl);
    free(file);

    return 0;
}