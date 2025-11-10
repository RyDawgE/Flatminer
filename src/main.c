#include "stdio.h"
#include "miner\miner.h"

int main () {
    char* path = "..\\data\\a.bin";
    printf("Hello, Flatminer!\n");
    printf("Mining |%s|\n", path);


    FlatbufferFile* file = malloc(sizeof(FlatbufferFile));
    alloc_flatbuffer(file, path);
    file->root_table.data = file->data + *(u32*)file->data;
    file->root_table.layer = 0;
    file->root_table.name = "Root";

    //printf("%d\n", is_valid_table(file->root_table.data));

    analyze_table(file, file->root_table);
    //analyze_table_as_type(file, file->root_table, 4, FIELD_OBJECT_ARR);

    //printf("IS VALID TABLE: %d\n", is_valid_table(file, file->root_table.data));

    FlatbufferTable t2 = file->root_table;
    t2.data = file->data +0x38 + *(u32*)(file->data+0x38);
    t2.layer = 1;
    t2.name = "Sub";

    //analyze_table(file, t2);
    //printf("IS VALID TABLE: %d\n", is_valid_table(file, t2.data));


    FlatbufferTable t3 = file->root_table;
    t3.data = file->data+0x76F4;
    t3.layer = 2;
    t3.name = "Sub 2";

    //analyze_table(file, t3);
    //printf("IS VALID TABLE: %d\n", is_valid_table(file, t3.data));


    return 0;
}