#include "stdio.h"
#include "miner\miner.h"

int main () {
    char* path = "..\\data\\a.bin";
    printf("Hello, Flatminer!\n");
    printf("Mining |%s|", path);


    FlatbufferFile* root = malloc(sizeof(FlatbufferFile));
    alloc_flatbuffer(root, path);
    set_table_pointers(root);
    read_vtable(root);

    free(root->data);
    free(root);

    return 0;
}