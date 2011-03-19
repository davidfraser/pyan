#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "fs.h"
#include "fs-internal.h"


int total_pins;

int cache_lookups = 0;
int cache_lookup_cost = 0;


void pin(BLOCK *b)
{
    b->pins++;
    //printf("Pin %ld\n", b->location);
    if (b->pins > MAX_PINS)
    {
        error("Block %d pinned too many (%d) times!", b->location, b->pins);
    }
    total_pins++;
}


void unpin(BLOCK *b)
{
    b->pins--;
    //printf("Unpin %ld\n", b->location);
    if (b->pins < 0)
    {    
        error("Block %d unpinned too many times!", b->location);
    }
    total_pins--;
}


static void populate_block(FS *fs, BLOCK *block)
{
    struct block_header *bh = (struct block_header *) block->buffer;
    
    if (block->type == B_DATA || block->type == B_FREE)
        return;
    
    bh->version = 1;
    bh->location = block->location;
    bh->type = block->type;
    
    if (block->type == B_SUPER)
    {
        struct superblock_header *sbh = (struct superblock_header *) block->buffer;
        
        sbh->block_size = fs->block_size;
        sbh->root_location = fs->root_location;
        sbh->next_label = fs->next_label;
        sbh->max_bitmap_pointers = fs->max_bitmap_pointers;
        sbh->num_bitmap_pointers = fs->num_bitmap_pointers;
        sbh->bitmap_size = fs->bitmap_size;
    }
    
    set_flag(block, F_DIRTY);
}


void flush_block(FS *fs, BLOCK *block)
{
    if (block->flags & F_CACHED && block->flags & F_DIRTY)
    {
        size_t nw;

        //printf("Saving block %d\n", block->location);
        populate_block(fs, block);
        if (fseek(fs->f, block->location * fs->block_size, SEEK_SET))
        error("Error seeking for writing block %d (written %d)", block->location, fs->blocks_written);
        
        //printf("W %d %p, %d\n", block->location, fs->f, fileno(fs->f));
        if (is_watched(block->location))
        {
            printf("WRITE ");
            print_block(fs, block);
        }
        nw = fwrite(block->buffer, fs->block_size, 1, fs->f);
        if (nw < 1)
        error("Error saving block!\n");
        
        clear_flag(block, F_DIRTY);
    }
}


static void add_block_to_hash(FS *fs, BLOCK *block)
{
    int hash = block->location % fs->cache_hash_size;
    //printf("Adding block %ld to cache\n", block->location);
    block->hash_next = fs->cache_hash[hash];
    fs->cache_hash[hash] = block;
}


static void remove_block_from_hash(FS *fs, BLOCK *block)
{
    BLOCK *ptr;
    int hash = block->location % fs->cache_hash_size;
    
    if (!(block->flags & F_CACHED))
        return;
    
    ptr = fs->cache_hash[hash];
    if (ptr == block)
    {
        //printf("Removing block %ld from cache\n", block->location);
        fs->cache_hash[hash] = block->hash_next;
        return;
    }
    while (ptr != NULL)
    {
        if (ptr->hash_next == block)
        {
            //printf("Removing block %ld from cache\n", block->location);
            ptr->hash_next = block->hash_next;
            return;
        }
        
        ptr = ptr->hash_next;
    }
    
    //printf("Block %ld not removed from cache\n", block->location);
}


static BLOCK *find_free_slot(FS *fs)
{
    int attempts = 0, max = 3;
    
    while (attempts < max)
    {
        BLOCK *ptr = fs->cache_tail;
        attempts++;
    
        while (ptr != NULL)
        {
            if (!(ptr->flags & F_CACHED)
                    || (!(ptr->flags & F_DIRTY) && ptr->pins == 0))
            {
                flush_block(fs, ptr);
                remove_block_from_hash(fs, ptr);
                ptr->flags = 0;
                set_flag(ptr,  F_CACHED);
                ptr->pins = 0;
                return ptr;
            }
            
            ptr = ptr->prev;
        }
        
        flush_fs(fs);
    }
    
    error("No more free slots in cache -- increase size or find bug");
    return NULL;
}


static int parse_block(FS *fs, BLOCK *block)
{
    struct block_header *bh = (struct block_header *) block->buffer;
    
    if (bh->version != 1)
    {
        printf("Error, version was %ld\n", bh->version);
        return 0;
    }
    
    if (bh->location != block->location)
    {
        printf("Error, location was %ld, expected %ld\n", bh->location, block->location);
        return 0;
    }
    
    block->type = bh->type;
    
    if (bh->type == B_SUPER)
    {
        struct superblock_header *sbh = (struct superblock_header *) block->buffer;
        fs->block_size = sbh->block_size;
        fs->root_location = sbh->root_location;
        fs->next_label = sbh->next_label;
        fs->max_bitmap_pointers = sbh->max_bitmap_pointers;
        fs->num_bitmap_pointers = sbh->num_bitmap_pointers;
        fs->bitmap_size = sbh->bitmap_size;
        fs->bitmaps = (unsigned long int *) (block->buffer + sizeof(struct superblock_header));
    }
    
    return 1;
}


static BLOCK *read_block(FS *fs, int location, int parse)
{
    size_t nr;
    BLOCK *block = find_free_slot(fs);
    //printf("R %d %p, %d\n", location, fs->f, fileno(fs->f));
    if (fseek(fs->f, location * fs->block_size, SEEK_SET))
        error("Error seeking for reading block %d", location);
    
    nr = fread(block->buffer, fs->block_size, 1, fs->f);
    if (nr < 1)
    {
        clear_flag(block, F_CACHED);
        if (location != 0)
            printf("Error reading block %d, nr = %d, errno was %d\n", location, nr, errno);
        return NULL;
    }
    block->location = location;
    set_flag(block, F_CACHED);
    block->type = B_DATA;
    block->pins = 0;
    
    if (parse)
    {
        if (!parse_block(fs, block))
        {
            printf("Error parsing block %d\n", location);
            return NULL;
        }
    }
    
    if (is_watched(block->location))
    {
        printf("READ ");
        print_block(fs, block);
    }
    add_block_to_hash(fs, block);

    return block;
}


static void bitmap_set(FS *fs, LOCATION location, int allocated)
{
    unsigned long int bitmap_num = location / fs->bitmap_size;
    unsigned long int bitmap_offset = location % fs->bitmap_size;
    unsigned long int bitmap_byte = bitmap_offset / 8;
    unsigned long int bitmap_bit = bitmap_offset % 8;
    BLOCK *bitmap;
    unsigned char *byte;
    int prev;

    if (fs->bitmaps[bitmap_num] == 0)
    {
        bitmap = allocate_block(fs, B_BITMAP, 0);
        fs->bitmaps[bitmap_num] = bitmap->location;
        bitmap_set(fs, bitmap->location, 1);
        set_flag(fs->superblock, F_DIRTY);
    }
    else
        bitmap = get_block(fs, fs->bitmaps[bitmap_num], 1);
    
    byte = (unsigned char *) bitmap->buffer + sizeof(struct bitmap_block_header) + bitmap_byte;
    prev = (*byte & (1 << bitmap_bit)) != 0;
    if (allocated)
        *byte = *byte | (1 << bitmap_bit);
    else
        *byte = *byte & ~(1 << bitmap_bit);
    set_flag(bitmap, F_DIRTY);
    
    if (prev == allocated)
        error("Not toggling bit!\n");
    
    //printf("Set bit %ld to %d from %d\n", location, allocated, prev);
}


static LOCATION bitmap_search(FS *fs, LOCATION target)
{
    unsigned long int bitmap_num = target / fs->bitmap_size;
    unsigned long int bitmap_offset = target % fs->bitmap_size;
    unsigned long int bitmap_byte = bitmap_offset / 8;
    unsigned long int bitmap_bit = bitmap_offset % 8;
    
    BLOCK *bitmap;
    
    if (fs->bitmaps[bitmap_num] == 0)
        return 0;

    bitmap = get_block(fs, fs->bitmaps[bitmap_num], 1);
    
    while (bitmap_byte*8 < fs->bitmap_size && target < fs->num_blocks)
    {
        unsigned char *byte = (unsigned char *) bitmap->buffer + sizeof(struct bitmap_block_header) + bitmap_byte;
        if (!(*byte & (1 << bitmap_bit)))
        {
            //printf("Found free slot at %ld\n", target);
            return target;
        }
        
        target++;
        bitmap_bit++;
        if (bitmap_bit >= 8)
        {
            bitmap_bit = 0;
            bitmap_byte++;
        }
    }
    
    /* No free block found. */
    return 0;
}


BLOCK *allocate_block(FS *fs, BLOCK_TYPE type, LOCATION target)
{
    BLOCK *block;
    LOCATION addr;
    
    if (type != B_SUPER)
        addr = bitmap_search(fs, target);
    else
        addr = 0;

    if (addr != 0)
    {
        struct superblock_header *sbh;

        block = get_block(fs, addr, 0);
        sbh = (struct superblock_header *) fs->superblock->buffer;
        sbh->block_counts[B_FREE]--;
        if (!block)
            error("Error reusing free block!\n");
        remove_block_from_hash(fs, block);
    }
    else
    {
        block = find_free_slot(fs);
        if (!block)
            error("Error allocating block, cache is full!\n");
        
        addr = fs->num_blocks;
        fs->num_blocks++;
    }
    
    memset(block->buffer, 0, fs->block_size);
    set_flag(block, F_CACHED);
    set_flag(block, F_DIRTY);
    block->location = addr;
    block->type = type;
    block->pins = 0;
    
    if (block->type != B_DATA)
    {
        struct block_header *h = (struct block_header *) block->buffer;
        h->version = 1;
        h->location = addr;
        h->type = type;
    }
    
    if (block->type == B_TREE)
    {
        struct tree_block_header *h = (struct tree_block_header *) block->buffer;
        h->height = 0;
        h->num_keys = 0;
        h->string_size = 0;
    }
    
    if (block->type == B_SUPER)
    {
        struct superblock_header *sbh = (struct superblock_header *) block->buffer;
        sbh->block_counts[B_SUPER] = 1;
        fs->max_bitmap_pointers = (fs->block_size - sizeof (struct superblock_header)) / sizeof (unsigned long int);
        fs->num_bitmap_pointers = 0;
        fs->bitmap_size = (fs->block_size - sizeof (struct superblock_header)) * 8;
        fs->bitmaps = (unsigned long int *) (block->buffer + sizeof(struct superblock_header));
        fs->superblock = block;
    }
    else
    {
        struct superblock_header *sbh = (struct superblock_header *) fs->superblock->buffer;
        sbh->block_counts[block->type]++;
    }
    
    flush_block(fs, block);
    set_flag(block, F_CACHED);
    set_flag(block, F_DIRTY);
    add_block_to_hash(fs, block);
    
    if (block->type != B_BITMAP)
        bitmap_set(fs, addr, 1);
    
    //printf("Allocated block %d, type %d\n", block->location, block->type);
    return block;
}


void deallocate_block(FS *fs, BLOCK_TYPE type, LOCATION location)
{
    struct superblock_header *sbh;

    if (location < 1 || location >= fs->num_blocks)
        error("Error, deallocating block %d that's not in filesystem (valid range is %d to %d)",
                location, 0, fs->num_blocks);

    sbh = (struct superblock_header *) fs->superblock->buffer;
    sbh->block_counts[type]--;
    type = B_FREE;
    sbh->block_counts[type]++;

    bitmap_set(fs, location, 0);
}


static BLOCK *find_block_in_cache(FS *fs, LOCATION location)
{
    int hash = location % fs->cache_hash_size;
    BLOCK *ptr = fs->cache_hash[hash];
    //BLOCK *ptr = fs->cache_head;
    cache_lookups++;
    while (ptr != NULL)
    {
        cache_lookup_cost++;
        if (ptr->flags & F_CACHED
                && !(ptr->flags & F_CLONE)
                && ptr->location == location)
        {
            //printf("Found block %ld in cache\n", ptr->location);
            return ptr;
        }
        
        ptr = ptr->hash_next;
    }

    //printf("Block %ld not found in cache\n", location);
    return NULL;
}


static void move_block_to_front(FS *fs, BLOCK *b)
{
    /* First remove b from the list. */
    if (b->prev)
        b->prev->next = b->next;
    else
        fs->cache_head = b->next;
    
    if (b->next)
        b->next->prev = b->prev;
    else
        fs->cache_tail = b->prev;
    
    /* Then add it at the front. */
    b->prev = NULL;
    b->next = fs->cache_head;
    fs->cache_head->prev = b;
    fs->cache_head = b;
}


BLOCK *get_block(FS *fs, LOCATION location, int parse)
{
    BLOCK *b = find_block_in_cache(fs, location);
    if (b == NULL)
        b = read_block(fs, location, parse);
    if (b == NULL)
        return NULL;
    
    move_block_to_front(fs, b);    
    return b;
}


BLOCK *clone_block(FS *fs, BLOCK *block)
{
    BLOCK *clone;
    
    if (block->pins == 0 && !(block->flags & F_DIRTY))
    {
        set_flag(block, F_CLONE);
        remove_block_from_hash(fs, block);
        return block;
    }
    
    clone = find_free_slot(fs);
    memcpy(clone->buffer, block->buffer, fs->block_size);
    set_flag(clone, F_CACHED);
    set_flag(clone, F_CLONE);
    clone->type = block->type;
    clone->location = block->location;
    if (block->type != B_DATA)
    {
        if (!parse_block(fs, clone))
        {
            printf("Error parsing cloned block %ld\n", block->location);
            return NULL;
        }
    }
    
    return clone;
}


void unclone_block(FS *fs, BLOCK *block)
{
    //nothing
}
