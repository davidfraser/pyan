#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "fs.h"
#include "fs-internal.h"


int fs_watch_mode = 0;


int is_watched(LOCATION location)
{
    return fs_watch_mode /*&& location == 3*/;
}


void error(char *fmt, ...)
{
    char buffer[1000];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    printf("%s\n", buffer);
    *((char *) NULL) = 0;
    exit(1);
}


void print_block(FS *fs, BLOCK *block)
{
    int i;
    
    printf("BLOCK type %d, flags %d, pins %d, location %ld (buffer %p)\n",
            block->type, block->flags, block->pins, block->location, block->buffer);

    if (block->type == B_SUPER)
    {
        struct superblock_header *sbh = (struct superblock_header *) block->buffer;
        printf("    SUPER block_size %ld, root_location %ld\n",
                sbh->block_size, sbh->root_location);
        printf("    blocks counts");
        for (i = 0; i < NUM_BLOCK_TYPES; i++)
            printf(" %ld", sbh->block_counts[i]);
        printf("\n");
    }
    
    if (block->type == B_TREE)
    {
        struct tree_block_header *h = (struct tree_block_header *) block->buffer;
        printf("    TREE height %d, num_keys %d, string_size %d\n", h->height, h->num_keys, h->string_size);
        print_keys(fs, block);
    }
}


void print_tree(FS *fs, BLOCK *block, int indent)
{
    char temp_spacing[100];
    int i;
    
    pin(block);
    
    strcpy(temp_spacing, "");
    for (i = 0; i < indent; i++)
        strcat(temp_spacing, "    ");
    
    printf("%sTREE type %d location %ld\n", temp_spacing, block->type, block->location);

    if (block->type == B_SUPER)
    {
        BLOCK *b = get_block(fs, fs->root_location, 1);
        print_tree(fs, b, indent+1);
    }
    else
    {
        struct tree_block_header *h = (struct tree_block_header *) block->buffer;
        INDEX index;
        int i;
        
        get_index(fs, block, &index);
        
        for (i = 0; i < h->num_keys; i++)
        {
            BLOCK *b;
            printf("%s    ", temp_spacing);
            printf("KEY '%s', type %d, data %ld\n", index.strings + index.offsets[i], index.types[i], index.data[i]);
            
            if (index.types[i] != K_NODE && index.types[i] != K_MIN_KEY)
                continue;
            b = get_block(fs, index.data[i], 1);
            print_tree(fs, b, indent+2);
        }
    }
    
    unpin(block);
}


static void initialise_fs(FS *fs)
{
    int i;
    int last_block_size;
    int attempts = 0;

reread:
    attempts++;
    if (attempts > 2)
        error("Too many attempts!");
    
    /* Create the block cache. */
    fs->cache_size = MAX2(DEFAULT_TOTAL_CACHE_SIZE/fs->block_size, MIN_CACHE_SIZE);
    fs->cache = malloc(sizeof(BLOCK) * fs->cache_size);
    fs->buffer = malloc(fs->block_size * fs->cache_size);
    
    for (i = 0; i < fs->cache_size; i++)
    {
        BLOCK *block = &fs->cache[i];
        block->flags = 0;
        block->pins = 0;
        block->buffer = fs->buffer + fs->block_size*i;
        block->next = (i < fs->cache_size - 1) ? &fs->cache[i + 1] : NULL;
        block->prev = (i > 0) ? &fs->cache[i - 1] : NULL;
        block->hash_next = NULL;
    }
    fs->cache_head = &fs->cache[0];
    fs->cache_tail = &fs->cache[fs->cache_size - 1];
    
    fs->cache_hash_size = DEFAULT_CACHE_HASH_SIZE;
    fs->cache_hash = malloc(sizeof(BLOCK *) * fs->cache_hash_size);
    for (i = 0; i < fs->cache_hash_size; i++)
        fs->cache_hash[i] = NULL;
    
    /* Load/create the superblock and root directory. */
    last_block_size = fs->block_size;
    fs->superblock = get_block(fs, 0, 1);
    if (fs->superblock != NULL)
    {
        if (fs->block_size != last_block_size)
        {
            free(fs->cache);
            free(fs->buffer);
            goto reread;
        }
        fseek(fs->f, 0, SEEK_END);
        fs->num_blocks = ftell(fs->f) / fs->block_size;
        fs->blocks_written = fs->num_blocks;
    }
    else
    {
        BLOCK *root_block;
        fs->superblock = allocate_block(fs, B_SUPER, 0);
        root_block = allocate_block(fs, B_TREE, 0);
        fs->root_location = root_block->location;
        fs->next_label = 1;
    }
    total_pins = 0;
    pin(fs->superblock);
    
    /* Create the handle cache. */
    fs->fh_cache_size = DEFAULT_FH_CACHE_SIZE;
    fs->fh_cache = malloc(sizeof(FH) * fs->fh_cache_size);
    
    for (i = 0; i < fs->fh_cache_size; i++)
    {
        fs->fh_cache[i].flags = 0;
    }
}


FS *create_fs(const char *filename, int block_size)
{
    FS *fs = malloc(sizeof(FS));
    
    remove(filename);
    fs->filename = strdup(filename);
    
    fs->f = fopen(filename, "wb+");
    if (!fs->f)
    {
        free(fs->filename);
        free(fs);
        return NULL;
    }
    
    fs->num_blocks = 0;
    fs->block_size = block_size;

    initialise_fs(fs);
    return fs;
}


FS *open_fs(const char *filename)
{
    FS *fs = malloc(sizeof(FS));
    
    fs->filename = strdup(filename);
    
    fs->f = fopen(filename, "rb+");
    if (!fs->f)
    {
        free(fs->filename);
        free(fs);
        return NULL;
    }
    
    fs->num_blocks = 0;
    fs->block_size = MIN_BLOCK_SIZE;
    
    initialise_fs(fs);
    
    return fs;
}


int block_location_cmp(const void *a, const void *b)
{
    BLOCK *blocka = *(BLOCK **) a;
    BLOCK *blockb = *(BLOCK **) b;
    return blocka->location - blockb->location;
}


void flush_fs(FS *fs)
{
    BLOCK *order[MAX_CACHE_SIZE];
    int count = 0;
    int i;
    
    set_flag(fs->superblock, F_DIRTY);
    
    for (i = 0; i < fs->cache_size; i++)
        if (fs->cache[i].flags & F_CACHED && fs->cache[i].flags & F_DIRTY)
        {
            order[count] = &fs->cache[i];
            count++;
        }
    
    qsort(order, count, sizeof (BLOCK *), block_location_cmp);

    for (i = 0; i < count; i++)
        flush_block(fs, order[i]);

    fflush(fs->f);
}


void close_fs(FS *fs)
{
    flush_fs(fs);
    print_block(fs, fs->superblock);
    unpin(fs->superblock);
    
    free(fs->filename);
    fclose(fs->f);
    free(fs->cache);
    free(fs->buffer);
    free(fs);

    if (total_pins != 0)
        error("Total pins is %d instead of zero!", total_pins);
    
    printf("cache_lookups = %d, cache_lookup_cost = %d\n", cache_lookups, cache_lookup_cost);
    printf("key_iterations = %d, key_iteration_cost = %d\n", key_iterations, key_iteration_cost);
}


FH *find_free_handle_slot(FS *fs)
{
    int i;
    int pos = rand() % fs->fh_cache_size;
    
    for (i = 0; i < fs->fh_cache_size; i++)
    {
        int p = (pos+i) % fs->fh_cache_size;
        if (!(fs->fh_cache[p].flags & F_CACHED))
            return &fs->fh_cache[p];
    }
    
    error("Out of free handle slots!");
    return NULL;
}
