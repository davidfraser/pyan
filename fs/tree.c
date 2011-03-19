#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fs.h"
#include "fs-internal.h"


int key_iterations = 0;
int key_iteration_cost = 0;


static size_t copy_shortest_name(char *dest, const char *k1, const char *k2)
{
    size_t len = 0;
    
    while (*k1 && *k2 && *k1 == *k2)
    {
        *dest = *k2;
        k1++;
        k2++;
        dest++;
        len++;
    }
    if (*k2)
    {
        *dest = *k2;
        dest++;
        len++;
    }
    *dest = 0;
    
    return len;
}


void print_index(INDEX *index)
{
    struct tree_block_header *h = (struct tree_block_header *) index->block->buffer;
    int i;
    
    printf("INDEX strings = %d, types = %d, offsets = %d, data = %d\n",
            (char *) index->strings - index->block->buffer, (char *) index->types - index->block->buffer, (char *) index->offsets - index->block->buffer, (char *) index->data - index->block->buffer);
    if (h->height == 0)
    {
        printf("    TYPES =");
        for (i = 0; i < h->num_keys; i++)
            printf(" %d", index->types[i]);
        printf("\n");
    }
    printf("    OFFSETS =");
    for (i = 0; i < h->num_keys; i++)
        printf(" %d", index->offsets[i]);
    printf("\n");
    printf("    DATA =");
    for (i = 0; i < h->num_keys; i++)
        printf(" %ld", index->data[i]);
    printf("\n");
}


void get_index(FS *fs, BLOCK *block, INDEX *index)
{
    struct tree_block_header *h = (struct tree_block_header *) block->buffer;
    
    index->block = block;
    index->strings = block->buffer + sizeof (struct tree_block_header);
    index->data = (unsigned long int *) (block->buffer + fs->block_size - h->num_keys * sizeof(unsigned long int));
    index->offsets = (unsigned short int *) (((char *) index->data) - h->num_keys * sizeof(unsigned short int));
    
    if (h->height == 0)
        index->types = (unsigned char *) (((char *) index->offsets) - h->num_keys * sizeof(unsigned char));
    else
        index->types = (void *) (index->offsets);
}


static void get_key(INDEX *index, int pos, KEY *key)
{
    key->pos = pos;
    key->type = index->types[pos];
    key->name = index->strings + index->offsets[pos];
    key->pointer = index->data[pos];
}


/**
 * Return the index of the maximum key in @a block less than or equal to @a name.
 */
int find_key(INDEX *index, const char *name)
{
    struct tree_block_header *h = (struct tree_block_header *) index->block->buffer;
    int lo, hi;
    
    lo = 0;
    hi = h->num_keys - 1;
    
    key_iterations++;
    
    while (lo <= hi)
    {
        int pos = (lo + hi) / 2;
        char *str = index->strings + index->offsets[pos];
        int r = COMPARE_FUNCTION(str, name);
        
        key_iteration_cost++;
        
        if (r == 0)
            return pos;
        else if (r < 0)
            lo = pos + 1;
        else
            hi = pos - 1;        
    }
    
    return hi;
}


/** Calculate the free space in a block, using its index. */
int get_free_space(INDEX *index)
{
    struct tree_block_header *h = (struct tree_block_header *) index->block->buffer;
    
    /* Free space is the space between the last string and the types. */
    char *end = index->types;
    char *start = index->strings + h->string_size;
    
    return end - start;
}


/** Calculate the space required for a key. */
int get_required_space(const char *name, int height)
{
    /* Space is (name+1) + type (if height == 0) + 2 + 4. */
    int h = (height == 0) ? 1 : 0;
    return strlen(name) + 1 + h + 2 + 4;
}


/** Insert a key into a block, using its index. */
int add_key_to_index(FS *fs, INDEX *index, const char *name, char type, unsigned long int data)
{
    struct tree_block_header *h = (struct tree_block_header *) index->block->buffer;
    int free = get_free_space(index);
    int required = get_required_space(name, h->height);
    int pos, offset;
    char *str_pos;
    unsigned short int *new_offsets;
    unsigned long int *new_data;
    
    if (required > free)
        error("Not enough space for key!");
    
    pos = find_key(index, name);
    if (pos >= 0 && strcmp(index->strings + index->offsets[pos], name) == 0)
        error("Key already exists!");
    
    pos++;
    
    /* Append the name. */
    str_pos = index->strings + h->string_size;
    strcpy(str_pos, name);
    h->string_size += strlen(name) + 1;
    offset = str_pos - index->strings;

    /* First move types (if applicable) */
    if (h->height == 0)
    {
        unsigned char *new_types = index->types - (1 + 2 + 4);
        memmove(new_types, index->types, pos);
        new_types[pos] = type;
        memmove(new_types + pos + 1, index->types + pos, h->num_keys - pos);
        index->types = new_types;
    }
    
    /* Then move offsets. */
    new_offsets = ((char *) index->offsets) - (2 + 4);
    memmove(new_offsets, index->offsets, pos * 2);
    new_offsets[pos] = offset;
    memmove(new_offsets + pos+1, index->offsets + pos, (h->num_keys - pos)*2);
    index->offsets = new_offsets;
    
    /* Then move data. */
    new_data = ((char *) index->data) - (4);
    memmove(new_data, index->data, pos * 4);
    new_data[pos] = data;
    index->data = new_data;

    h->num_keys++;
    
    return pos;
}


/** Remove a key from a block, using its index. */
int remove_key_from_index(FS *fs, INDEX *index, const char *name)
{
    struct tree_block_header *h = (struct tree_block_header *) index->block->buffer;
    int offset;
    unsigned long int *new_data;
    unsigned short int *new_offsets;
    char *str_pos;
    int len;
    int i;
    
    int pos = find_key(index, name);
    if (pos < 0 || strcmp(index->strings + index->offsets[pos], name) != 0)
        error("Key isn't in index!");
    
    offset = index->offsets[pos];

    /* Moving the lists is almost the reverse process to that used for adding
       keys.  First move data. */
    new_data = ((char *) index->data) + (4);
    memmove(new_data, index->data, pos * 4);
    index->data = new_data;
    
    /* Then move offsets. */
    new_offsets = ((char *) index->offsets) + (2 + 4);
    memmove(new_offsets + pos, index->offsets + pos + 1, (h->num_keys - pos - 1)*2);
    memmove(new_offsets, index->offsets, pos * 2);
    index->offsets = new_offsets;

    /* Then move types (if applicable) */
    if (h->height == 0)
    {
        unsigned char *new_types = index->types + (1 + 2 + 4);
        memmove(new_types + pos, index->types + pos + 1, h->num_keys - pos - 1);
        memmove(new_types, index->types, pos);
        index->types = new_types;
    }
    
    /* Remove the name.  This means compacting the strings. */
    str_pos = index->strings + offset;
    len = strlen(str_pos) + 1;
    memmove(str_pos, str_pos+len, h->string_size - offset - len);
    h->string_size -= len;
    
    h->num_keys--;
    
    /* Fix the offsets of strings that were moved. */
    for (i = 0; i < h->num_keys; i++)
        if (index->offsets[i] > offset)
            index->offsets[i] -= len;

    return pos;
}


void print_keys(FS *fs, BLOCK *block)
{
    struct tree_block_header *h = (struct tree_block_header *) block->buffer;
    INDEX index;
    int i;
    
    get_index(fs, block, &index);
    for (i = 0; i < h->num_keys; i++)
    {
        if (h->height == 0)
            printf("KEY '%s', type %d, data %ld\n", index.strings + index.offsets[i], index.types[i], index.data[i]);
        else
            printf("KEY '%s', data %ld\n", index.strings + index.offsets[i], index.data[i]);
    }
}


/** Split an index into two new indices.
 * @returns The position of the pivot key in the source index.
 */
int split_index(FS *fs, INDEX *source, INDEX *dest1, INDEX *dest2)
{
    struct tree_block_header *h = (struct tree_block_header *) source->block->buffer;
    int i = 0;
    int len = 0;
    int half_way = h->string_size / 2;
    int pivot;
    
    while (len < half_way)
    {
        add_key_to_index(fs, dest1, source->strings + source->offsets[i], source->types[i], source->data[i]);
        len += strlen(source->strings + source->offsets[i]) + 1;
        i++;
    }
    
    pivot = i;
    if (h->height > 0)
    {
        add_key_to_index(fs, dest2, source->strings + source->offsets[0], source->types[0], source->data[i]);
        i++;
    }
    
    while (i < h->num_keys)
    {
        add_key_to_index(fs, dest2, source->strings + source->offsets[i], source->types[i], source->data[i]);
        i++;
    }
    
    return pivot;
}


/** Merge two indexes into a new index.
 */
void merge_indexes(FS *fs, INDEX *dest, INDEX *source1, const char *pivot_key, INDEX *source2)
{
    struct tree_block_header *header1 = (struct tree_block_header *) source1->block->buffer;
    struct tree_block_header *header2 = (struct tree_block_header *) source2->block->buffer;
    
    int i;
    
    for (i = 0; i < header1->num_keys; i++)
    {
        add_key_to_index(fs, dest, source1->strings + source1->offsets[i], source1->types[i], source1->data[i]);
    }
    
    for (i = 0; i < header2->num_keys; i++)
    {
        const char *str = source2->strings + source2->offsets[i];
        
        if (i == 0 && header2->height > 0 && str[0] == 0)
            str = pivot_key;
        
        add_key_to_index(fs, dest, str, source2->types[i], source2->data[i]);
    }
}


/**
 * Split the @a original block in half, with the second half of the keys moved into @a effect->new_block.
 *
 * @note Both elements of effect->new_blocks will be pinned and must be unpinned when the effect has been processed.
 */
static void split_block(FS *fs, BLOCK *original, EFFECT *effect)
{
    struct tree_block_header *h = (struct tree_block_header *) original->buffer;
    struct tree_block_header *new_header0;
    struct tree_block_header *new_header1;
    int pivot;
    INDEX index;
    INDEX indexes[2];
   
    pin(original);
    
    effect->type = E_SPLIT;
    effect->new_blocks[0] = allocate_block(fs, original->type, 0);
    effect->new_blocks[1] = allocate_block(fs, original->type, 0);
    pin(effect->new_blocks[0]);
    pin(effect->new_blocks[1]);
    
    if (is_watched(original->location)
            || is_watched(effect->new_blocks[0]->location) || is_watched(effect->new_blocks[1]->location))
    {
        printf("Going to split block %ld\n", original->location);
        print_block(fs, original);
    }
    
    get_index(fs, original, &index);
    
    new_header0 = (struct tree_block_header *) effect->new_blocks[0]->buffer;
    new_header1 = (struct tree_block_header *) effect->new_blocks[1]->buffer;
    new_header0->height = h->height;
    new_header1->height = h->height;
    
    get_index(fs, effect->new_blocks[0], &indexes[0]);
    get_index(fs, effect->new_blocks[1], &indexes[1]);
    
    pivot = split_index(fs, &index, &indexes[0], &indexes[1]);
    
    //TODO can't use shortened keys unless we shorten them against the
    //maximal *real* key less than it, which is found in the right-most leaf
    //descended from the predecessor.  either propagate this up after a split
    //as part of the effect, or go look it up from here.
    strcpy(effect->new_name, index.strings + index.offsets[pivot]);
    //copy_shortest_name(effect->new_name, prev_prev_key.name, prev_key.name);
    
    if (is_watched(original->location)
            || is_watched(effect->new_blocks[0]->location) || is_watched(effect->new_blocks[1]->location))
    {
        printf("Block %ld was split off into %ld and %ld; after split:\n",
                original->location, effect->new_blocks[0]->location, effect->new_blocks[1]->location);
        print_block(fs, original);
        print_block(fs, effect->new_blocks[0]);
        print_block(fs, effect->new_blocks[1]);
    }

    set_flag(effect->new_blocks[0], F_DIRTY);
    set_flag(effect->new_blocks[1], F_DIRTY);
    effect->pending_pins = 1;
    unpin(original);
}


void insert_key(FS *fs, BLOCK *block, const char *name, KEY_TYPE type, LOCATION data, EFFECT *effect)
{
    struct tree_block_header *h = (struct tree_block_header *) block->buffer;
    int free_space;
    int required = get_required_space(name, h->height);
    INDEX index;
    
    effect->type = E_NONE;
    
    pin(block);
    
    if (is_watched(block->location))
    {
        printf("Inserting key '%s' type %d data %ld into block %ld\n", name, type, data, block->location);
        print_block(fs, block);
    }
    
    get_index(fs, block, &index);
    free_space = get_free_space(&index);
    if (required > free_space)
    {
        int r;

        split_block(fs, block, effect);
        if (effect->type != E_SPLIT)
            error("Block %d split but effect was %d", block->location, effect->type);
        
        /* Figre out which of the new blocks the key should go in. */
        r = COMPARE_FUNCTION(name, effect->new_name);
        unpin(block);
        if (r >= 0)
            block = effect->new_blocks[1];
        else
            block = effect->new_blocks[0];
        pin(block);
    }
    
    /* Recheck free space. */
    get_index(fs, block, &index);
    free_space = get_free_space(&index);
    if (required > free_space)
        error("Error, not enough space even after splitting!");
    
    add_key_to_index(fs, &index, name, type, data);
    
    if (is_watched(block->location))
    {
        printf("After insertion:\n");
        print_block(fs, block);
    }
    
    set_flag(block, F_DIRTY);
    unpin(block);
}


void replace_key(FS *fs, BLOCK *block, const char *old_name, const char *name,
        KEY_TYPE type, LOCATION data, EFFECT *effect)
{
    INDEX index;

    pin(block);
    
    get_index(fs, block, &index);
    remove_key_from_index(fs, &index, old_name);
    
    set_flag(block, F_DIRTY);
    
    insert_key(fs, block, name, type, data, effect);
    
    unpin(block);
}


void update_key(FS *fs, BLOCK *block, const char *name, KEY_TYPE type, LOCATION data)
{
    struct tree_block_header *h = (struct tree_block_header *) block->buffer;
    int pos;
    INDEX index;

    pin(block);

    get_index(fs, block, &index);
    pos = find_key(&index, name);
    if (pos == -1)
        error("Can't update key that doesn't exist!");
    
    if (h->height == 0)
        index.types[pos] = type;
    
    index.data[pos] = data;
    
    set_flag(block, F_DIRTY);
    unpin(block);
}


BLOCK *merge_blocks(FS *fs, BLOCK *b1, char *kn, BLOCK *b2)
{
    BLOCK *new_block = allocate_block(fs, B_TREE, 0);
    INDEX index;
    struct tree_block_header *header;
    struct tree_block_header *header0;
    struct tree_block_header *header1;
    INDEX indexes[2];

    pin(new_block);
    
    if (is_watched(new_block->location) || is_watched(b1->location) || is_watched(b2->location))
    {
        printf("Merging blocks %ld and %ld into new block %ld\n",
                b1->location, b2->location, new_block->location);
        print_block(fs, b1);
        print_block(fs, b2);
    }
    
    get_index(fs, new_block, &index);
    header = (struct tree_block_header *) new_block->buffer;
    
    header0 = (struct tree_block_header *) b1->buffer;
    header1 = (struct tree_block_header *) b2->buffer;
    
    get_index(fs, b1, &indexes[0]);
    get_index(fs, b2, &indexes[1]);
    
    if (header0->height != header1->height)
        error("Error merging blocks of differing heights %d and %d!", header0->height, header1->height);
    header->height = header0->height;
    
    merge_indexes(fs, &index, &indexes[0], kn, &indexes[1]);
    
    if (is_watched(new_block->location) || is_watched(b1->location) || is_watched(b2->location))
    {
        printf("After merge:\n");
        print_block(fs, new_block);
    }
    
    deallocate_block(fs, B_TREE, b1->location);
    deallocate_block(fs, B_TREE, b2->location);
    
    return new_block;
}


void delete_key(FS *fs, BLOCK *block, const char *name, EFFECT *effect)
{
    INDEX index;
     
    effect->type = E_SHRINK;
    
    pin(block);
    
    if (is_watched(block->location))
    {
        printf("Deleting key '%s' from block %ld\n", name, block->location);
        print_block(fs, block);
    }
    
    get_index(fs, block, &index);
    remove_key_from_index(fs, &index, name);
    
    if (is_watched(block->location))
    {
        printf("After deletion\n");
        print_block(fs, block);
    }
        
    set_flag(block, F_DIRTY);
    unpin(block);
}


static int search_tree(FS *fs, const char *name, size_t name_len, BLOCK *block,
        EFFECT *effect, SEARCH_CALLBACK callback, void *baton)
{
    struct tree_block_header *h = (struct tree_block_header *) block->buffer;
    int result = 0;
    INDEX index;
    int pos;
    BLOCK *child_block;
    EFFECT sub_effect;
    
    effect->type = E_NONE;
    
    if (block == NULL)
        error("Error, searching in NULL block!");
    
    if (block->type != B_TREE)
        error("Error, searching in non-tree block!");
    
    pin(block);
    
    if (is_watched(block->location))
    {
        printf("Searching for '%s' in:\n", name);
        print_block(fs, block);
    }
    
    get_index(fs, block, &index);
    pos = find_key(&index, name);

    if (is_watched(block->location))
        printf("Found key at %d\n", pos);
    
    /* If we're at the leaf level, execute the callback here. */
    if (h->height == 0)
    {
        KEY key;
        
        get_key(&index, pos, &key);
        effect->type = E_NONE;

        result = callback(fs, block, &key, effect, baton);
        unpin(block);
        return result;
    }
    
    /* Otherwise, recurse. */
    child_block = get_block(fs, index.data[pos], 1);
    pin(child_block);
    result = search_tree(fs, name, name_len, child_block, &sub_effect, callback, baton);
    if (sub_effect.type == E_SPLIT)
    {
        update_key(fs, block, index.strings + index.offsets[pos], 0, sub_effect.new_blocks[0]->location);
        insert_key(fs, block, sub_effect.new_name, 0, sub_effect.new_blocks[1]->location, effect);
        unpin(sub_effect.new_blocks[0]);
        unpin(sub_effect.new_blocks[1]);
        deallocate_block(fs, B_TREE, child_block->location);
    }
    else if (sub_effect.type == E_SHRINK && pos > 0)
    {
        BLOCK *neighbour = get_block(fs, index.data[pos-1], 1);
        INDEX indexes[2];

        pin(neighbour);

        get_index(fs, neighbour, &indexes[0]);
        get_index(fs, child_block, &indexes[1]);
        if (get_free_space(&indexes[0]) + get_free_space(&indexes[1]) > RESIZE_THRESHOLD*2*fs->block_size)
        {
            BLOCK *new_block = merge_blocks(fs, neighbour, index.strings + index.offsets[pos], child_block);
            index.data[pos-1] = new_block->location;
            unpin(new_block);
            delete_key(fs, block, index.strings + index.offsets[pos], effect);
        }
        unpin(neighbour);
    }
    unpin(child_block);
    
    unpin(block);
    return result;
}


int search(FS *fs, const char *name, SEARCH_CALLBACK callback, void *baton)
{
    int result = 0;
    BLOCK *block = fs->superblock;
    EFFECT sub_effect;
    BLOCK *child_block;
    
    pin(block);
    
    child_block = get_block(fs, fs->root_location, 1);
    pin(child_block);
    result = search_tree(fs, name, strlen(name), child_block, &sub_effect, callback, baton);
    if (sub_effect.type == E_SPLIT)
    {
        BLOCK *new_root;
        struct tree_block_header *root_header;
        struct tree_block_header *child_header = (struct tree_block_header *) child_block->buffer;
        
        new_root = allocate_block(fs, B_TREE, 0);
        root_header = (struct tree_block_header *) new_root->buffer;
        root_header->height = child_header->height + 1;
        
        insert_key(fs, new_root, "", 0, sub_effect.new_blocks[0]->location, &sub_effect);
        insert_key(fs, new_root, sub_effect.new_name, 0, sub_effect.new_blocks[1]->location, &sub_effect);
        unpin(sub_effect.new_blocks[0]);
        unpin(sub_effect.new_blocks[1]);
        deallocate_block(fs, B_TREE, child_block->location);
        fs->root_location = new_root->location;
    }
    else if (sub_effect.type == E_SHRINK)
    {
        struct tree_block_header *header = (struct tree_block_header *) child_block->buffer;
        
        if (header->height > 0 && header->num_keys == 1)
        {
            INDEX index;
            get_index(fs, child_block, &index);
        
            fs->root_location = index.data[0];
            deallocate_block(fs, B_TREE, child_block->location);
        }
    }
    
    unpin(child_block);
    unpin(block);
    return result;
}


int traverse(FS *fs, BLOCK *block, const char *prefix, size_t prefix_len, TRAVERSE_CALLBACK callback, void *baton)
{
    INDEX index;
    struct tree_block_header *h;
    int i;
    
    if (block->type != B_TREE)
        return 0;
    
    block = clone_block(fs, block);
    pin(block);
    
    //printf("Traversing block %ld\n", block->location);
    h = (struct tree_block_header *) block->buffer;
    get_index(fs, block, &index);
    
    for (i = MAX2(find_key(&index, prefix), 0); i < h->num_keys; i++)
    {
        if (h->height == 0 && COMPARE_N_FUNCTION(index.strings + index.offsets[i], prefix, prefix_len) == 0)
        {
            if (is_watched(block->location))
            {
                printf("ITEM '%s'\n", index.strings + index.offsets[i]);
            }
            callback(index.strings + index.offsets[i] + prefix_len, baton);
        }
        else if (h->height > 0)
        {
            BLOCK *child_block;
            int r;

            if (COMPARE_N_FUNCTION(index.strings + index.offsets[i], prefix, prefix_len) > 0)
                break;

            child_block = get_block(fs, index.data[i], 1);
            
            if (child_block == NULL)
            {
                print_block(fs, block);
                error("Invalid child block pointed to by %ld!", block->location);
            }
            
            r = traverse(fs, child_block, prefix, prefix_len, callback, baton);
            if (r != 0)
            {
                unpin(block);
                unclone_block(fs, block);
                return r;
            }
        }
    }
    
    unpin(block);
    unclone_block(fs, block);
    return 0;
}


int key_operation_callback(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data)
{
    struct key_operation_baton *baton = (struct key_operation_baton *) data;
    
    effect->type = E_NONE;
    
    if (key->pos >= 0 && !COMPARE_FUNCTION(key->name, baton->name))
    {
        if (baton->operation == O_FETCH)
        {
            baton->type = key->type;
            baton->data = key->pointer;
        }
        else if (baton->operation == O_REPLACE)
            update_key(fs, block, baton->name, baton->type, baton->data);
        else if (baton->operation == O_DELETE)
            delete_key(fs, block, baton->name, effect);
        else
            return 0;
    }
    else
    {
        if (baton->operation == O_INSERT || baton->operation == O_REPLACE)
            insert_key(fs, block, baton->name, baton->type, baton->data, effect);
        else
            return 0;
    }
    
    return 1;
}


int fetch_key(FS *fs, const char *name, KEY_TYPE *type, unsigned long int *data)
{
    struct key_operation_baton baton = { O_FETCH, name, 0, 0 };
    
    if (!search(fs, name, key_operation_callback, &baton))
        return 0;
    
    if (type)
        *type = baton.type;
    if (data)
        *data = baton.data;
    
    return 1;
}
