#include "stdio.h"
#include "miner\miner.h"


int main(int argc, char *argv[]) {
    char* path = "..\\data\\a.bin";

    if (argc < 2) {
        printf("ERROR: Please provide a valid filepath\n");
        return -1;
    } else {
        path = argv[1];
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
    generate_schema(file, tbl);

    free_table(tbl);
    free(file);

    return 0;
}