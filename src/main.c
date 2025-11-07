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

    analyze_table(file, file->root_table);
    //analyze_table_as_type(file, file->root_table, 4, FIELD_OBJECT_ARR);

    FlatbufferTable t2 = file->root_table;
    t2.data = file->data+112;
    t2.layer = 1;
    t2.name = "Sub";

    analyze_table(file, t2);



    return 0;
}