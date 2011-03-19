#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fs-internal.h"

#define TEST(n) do { \
    char *msg; \
    printf("Running: %s\n", #n); \
    msg = n(); \
    if (msg) { printf("FAILED in %s\n", msg); failures++; exit(1); } \
    else { printf("PASSED\n"); passes++; } \
} while (0)


static int block_size;


static FS fs;
static char buffer[65536];
static BLOCK block;
static struct tree_block_header *header;


static void setup(void)
{
    fs.block_size = block_size;
    block.type = B_TREE;
    block.buffer = buffer;
    
    header = (struct tree_block_header *) buffer;
    header->height = 0;
    header->num_keys = 0;
    header->string_size = 0;
}


static char *test_get_index_leaf(void)
{
    setup();
    
    INDEX index;
    get_index(&fs, &block, &index);
    
    if (index.types != (void *) (block.buffer + fs.block_size))
        return "Types list not at end of block!";
    
    if (index.offsets != (void *) (block.buffer + fs.block_size))
        return "Offsets list not at end of block!";
    
    if (index.data != (void *) (block.buffer + fs.block_size))
        return "Data list not at end of block!";
    
    return NULL;
}


#define NAME1 "0/test123"
#define NAME2 "0/test456"
#define NAME3 "0/test789"
#define TYPE1 1
#define TYPE2 2
#define TYPE3 3
#define DATA1 1001
#define DATA2 2002
#define DATA3 3003

static char *test_add_key_to_index_leaf(void)
{
    setup();
    
    INDEX index;
    
    get_index(&fs, &block, &index);
    add_key_to_index(&fs, &index, NAME1, TYPE1, DATA1);
    get_index(&fs, &block, &index);
    add_key_to_index(&fs, &index, NAME2, TYPE2, DATA2);
    get_index(&fs, &block, &index);
    add_key_to_index(&fs, &index, NAME3, TYPE3, DATA3);
    
    if (header->num_keys != 3)
        return "Incorrect number of keys!";

    if (header->string_size != strlen(NAME1)+1 + strlen(NAME2)+1 + strlen(NAME3)+1)
        return "Incorrect string size!";

    if (strcmp(index.strings + index.offsets[0], NAME1) != 0)
        return "Incorrect string!";
    
    if (index.types[0] != TYPE1)
        return "Incorrect type!";

    if (index.data[0] != DATA1)
        return "Incorrect data!";

    if (strcmp(index.strings + index.offsets[1], NAME2) != 0)
        return "Incorrect string!";
    
    if (index.types[1] != TYPE2)
        return "Incorrect type!";

    if (index.data[1] != DATA2)
        return "Incorrect data!";

    if (strcmp(index.strings + index.offsets[2], NAME3) != 0)
        return "Incorrect string!";
    
    if (index.types[2] != TYPE3)
        return "Incorrect type!";

    if (index.data[2] != DATA3)
        return "Incorrect data!";

    return NULL;
}


static char *test_remove_key_from_index_leaf(void)
{
    INDEX index;
    
    get_index(&fs, &block, &index);
    remove_key_from_index(&fs, &index, NAME1);
    get_index(&fs, &block, &index);
    remove_key_from_index(&fs, &index, NAME2);
    get_index(&fs, &block, &index);
    remove_key_from_index(&fs, &index, NAME3);
    
    if (header->num_keys != 0)
        return "Incorrect number of keys!";

    if (header->string_size != 0)
        return "Incorrect string size!";
    
    return NULL;
}


int test_main(int argc, char *argv[])
{
    int block_sizes[] = { 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
    int passes = 0, failures = 0;
    int i;

    for (i = 0; i < sizeof(block_sizes)/sizeof(int); i++)
    {
        block_size = block_sizes[i];
        printf("Running tests for block size %d\n", block_size);
        
        TEST(test_get_index_leaf);
        TEST(test_add_key_to_index_leaf);
        TEST(test_remove_key_from_index_leaf);
    }
    
    if (failures == 0)
        printf("All %d tests passed.\n", passes);
    else
        printf("Passes: %d, Failures: %d\n", passes, failures);
   
    return 0;
}
