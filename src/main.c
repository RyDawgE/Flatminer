#include "stdio.h"
#include "miner\miner.h"

int main () {
    printf("Hello, Flatminer!\n");

    FlatbufferFile* root = malloc(sizeof(FlatbufferFile));
    alloc_flatbuffer(root, "..\\data\\b.bin");
    
    printf("First table pointer: %u\n", root->data[0]);
    
    set_table_pointers(root);
    printf("First table value: %u\n", root->table[0]);
    printf("First vtable value: %u\n", root->vtable[0]);
    
    free(root->data);
    free(root);

    return 0;
}