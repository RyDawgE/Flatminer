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

        
    free(file);

    return 0;
}