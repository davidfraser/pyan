#include <string.h>
#include <stdio.h>
#include <time.h>

#include "fs-internal.h"


void add_labelled_key(FS *fs, unsigned long int label, const char *name, KEY_TYPE type, unsigned long int data)
{
    char temp[MAX_INTERNAL_NAME+1];
    struct key_operation_baton baton = { O_REPLACE, temp, type, data };
    
    sprintf(temp, "%ld/%s", label, name);
    
    search(fs, temp, key_operation_callback, &baton);
}


void delete_labelled_key(FS *fs, unsigned long int label, const char *name)
{
    char temp[MAX_INTERNAL_NAME+1];
    struct key_operation_baton baton = { O_DELETE, temp, 0, 0 };
    
    sprintf(temp, "%ld/%s", label, name);
    
    search(fs, temp, key_operation_callback, &baton);
}


int fetch_labelled_key(FS *fs, unsigned long int label, const char *name, KEY_TYPE *type, unsigned long int *data)
{
    char temp[MAX_INTERNAL_NAME+1];
    
    sprintf(temp, "%ld/%s", label, name);
    
    return fetch_key(fs, temp, type, data);
}


KEY_TYPE get_type(FS *fs, const char *name)
{
    unsigned long int label;
    KEY_TYPE type;
    
    find_dir_label(fs, name, &label, &name);
    if (!fetch_labelled_key(fs, label, name, &type, NULL))
        return 0;
    
    return type;
}


int get_attribute(FS *fs, const char *name, const char *attribute)
{
    unsigned long int label;
    unsigned long int attr;
    
    find_dir_label(fs, name, &label, &name);
    if (!fetch_labelled_key(fs, label, name, NULL, &label))
        return 0;
    if (!fetch_labelled_key(fs, label, attribute, NULL, &attr))
        return 0;
    
    return attr;
}


int set_attribute(FS *fs, const char *name, const char *attribute, KEY_TYPE type, unsigned long int data)
{
    unsigned long int label;
    
    find_dir_label(fs, name, &label, &name);
    if (!fetch_labelled_key(fs, label, name, NULL, &label))
        return 0;
    add_labelled_key(fs, label, attribute, type, data);
    
    return 1;
}


/**
 * Create a file key and its attribute subkeys.
 *
 * @return The file label.
 */
unsigned long int create_file(FS *fs)
{
    unsigned long int label = fs->next_label++;
    unsigned long int create_time = time(NULL);
    
    add_labelled_key(fs, label, "C", K_ATTRIBUTE, create_time);
    add_labelled_key(fs, label, "S", K_ATTRIBUTE, 0);
    
    return label;
}


/**
 * Create a directory key and its attribute subkeys.
 * @return The directory label.
 */
unsigned long int create_directory(FS *fs)
{
    unsigned long int label = fs->next_label++;
    unsigned long int prefix_label = fs->next_label++;
    unsigned long int create_time = time(NULL);
    
    add_labelled_key(fs, label, "C", K_ATTRIBUTE, create_time);
    add_labelled_key(fs, label, "D", K_ATTRIBUTE, prefix_label);
    
    return label;
}


void create_link(FS *fs, unsigned long int dir_label, const char *name,
        unsigned long int label, KEY_TYPE type)
{
    unsigned long int count;

    add_labelled_key(fs, dir_label, name, type, label);
    
    /* Increment the link count. */
    if (!fetch_labelled_key(fs, label, "L", NULL, &count))
        count = 0;
    add_labelled_key(fs, label, "L", K_ATTRIBUTE, count+1);
}


struct get_file_data_block_baton
{
    FH *fh;
    unsigned long int block_num;
    unsigned long int location;
    unsigned long int predecessor;
};


static int get_file_data_block_callback(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data)
{
    struct get_file_data_block_baton *baton = (struct get_file_data_block_baton *) data;
    char extent_prefix[MAX_INTERNAL_NAME+1];
    unsigned long int start = -1, stop = -1;
    BLOCK *b;

    sprintf(extent_prefix, "%ld/X", baton->fh->label);

    if (key->type != K_INVALID && strncmp(key->name, extent_prefix, strlen(extent_prefix)) == 0)
    {
        sscanf(key->name + strlen(extent_prefix), "%ld-%ld", &start, &stop);
        if (start <= baton->block_num && baton->block_num <= stop)
        {
            baton->location = key->pointer + (baton->block_num - start);
            return 1;
        }
    }
    
    b = allocate_block(fs, B_DATA, baton->predecessor);

    if (key->type != K_INVALID && strncmp(key->name, extent_prefix, strlen(extent_prefix)) == 0
            && stop == baton->block_num - 1 && key->pointer + (stop - start) == b->location - 1)
    {
        sprintf(extent_prefix, "%ld/X%08ld-%08ld", baton->fh->label, start, baton->block_num);
        replace_key(fs, block, key->name, extent_prefix, K_ATTRIBUTE, key->pointer, effect);
    }
    else
    {
        sprintf(extent_prefix, "%ld/X%08ld-%08ld", baton->fh->label, baton->block_num, baton->block_num);
        insert_key(fs, block, extent_prefix, K_ATTRIBUTE, b->location, effect);
    }
    
    baton->location = b->location;
    return 1;
}


BLOCK *get_file_data_block(FS *fs, FH *fh, unsigned long int num, unsigned long int predecessor)
{
    struct get_file_data_block_baton baton = { fh, num, 0, predecessor };
    char extent_prefix[MAX_INTERNAL_NAME+1];
    
    sprintf(extent_prefix, "%ld/X%08ld-99999999", fh->label, num);
    
    search(fs, extent_prefix, get_file_data_block_callback, &baton);
    
    return get_block(fs, baton.location, 0);
}


struct reduce_file_baton {
    unsigned long int num_blocks;
    unsigned long int new_num_blocks;
    unsigned long int label;
};


static int reduce_file_callback(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data)
{
    struct reduce_file_baton *baton = (struct reduce_file_baton *) data;
    char extent_prefix[MAX_INTERNAL_NAME+1];
    unsigned long int start, stop;
    unsigned long int first_to_go;
    unsigned long int i;
    
    sprintf(extent_prefix, "%ld/X", baton->label);

    if (key->type == K_INVALID || strncmp(key->name, extent_prefix, strlen(extent_prefix)) != 0)
        return 0;
    
    sscanf(key->name + strlen(extent_prefix), "%ld-%ld", &start, &stop);
    if (stop < baton->new_num_blocks)
    {
        baton->num_blocks = stop+1;
        return 0;
    }
    
    first_to_go = MAX2(start, baton->new_num_blocks);
    
    for (i = first_to_go; i <= stop; i++)
        deallocate_block(fs, B_DATA, key->pointer + (i - start));
    
    if (first_to_go <= start)
        delete_key(fs, block, key->name, effect);
    else
    {
        sprintf(extent_prefix, "%ld/X%08ld-%08ld", baton->label, start, first_to_go - 1);
        replace_key(fs, block, key->name, extent_prefix, K_ATTRIBUTE, key->pointer, effect);
    }
    
    baton->num_blocks = first_to_go;
    return 1;
}


void reduce_file(FS *fs, unsigned long int label, unsigned long int size, unsigned long int new_size)
{
    int num_blocks = SIZE_2_BLOCKS(size, fs->block_size);
    int new_num_blocks = SIZE_2_BLOCKS(new_size, fs->block_size);
    struct reduce_file_baton baton = { num_blocks, new_num_blocks, label };
    
    while (baton.num_blocks > baton.new_num_blocks)
    {
        char extent_prefix[MAX_INTERNAL_NAME+1];
    
        sprintf(extent_prefix, "%ld/X%08ld-99999999", label, baton.num_blocks-1);
    
        if (!search(fs, extent_prefix, reduce_file_callback, &baton))
            error("Unable to find extent containing block '%ld'!", baton.num_blocks - 1);
    }
}


/**
 * Delete all keys belonging to a file or directory.
 */
static void delete_file(FS *fs, unsigned long int label)
{
    unsigned long int size;
    if (fetch_labelled_key(fs, label, "S", NULL, &size))
        reduce_file(fs, label, size, 0);
    
    delete_labelled_key(fs, label, "A");
    delete_labelled_key(fs, label, "C");
    delete_labelled_key(fs, label, "M");
    delete_labelled_key(fs, label, "D");
    delete_labelled_key(fs, label, "S");
    delete_labelled_key(fs, label, "L");
}


int delete_link(FS *fs, unsigned long int dir_label, const char *name)
{
    unsigned long int label;
    unsigned long int count;
    
    if (!fetch_labelled_key(fs, dir_label, name, NULL, &label))
        return 0;
    
    delete_labelled_key(fs, dir_label, name);
    
    /* Decrement the link count and delete the actual file if this was the last link. */
    if (!fetch_labelled_key(fs, label, "L", NULL, &count))
        count = 0;
    
    if (count > 1)
        add_labelled_key(fs, label, "L", K_ATTRIBUTE, count-1);
    else
        delete_file(fs, label);
    
    return 1;
}


int get_label_callback(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data)
{
    if (key->type != K_INVALID && !COMPARE_FUNCTION(key->name, (char *) data))
        return key->pointer;
    
    return 0;
}


/**
 * Find the prefix label of a filesystem path.
 *
 * @param fs Open FS object
 * @param name full filename to find dir label for
 * @param pointer to output prefix
 * @param pointer to output short name
 *
 * @return 1 if directory exists, 0 otherwise
 */
int find_dir_label(FS *fs, const char *name, unsigned long int *prefix, const char **short_name)
{
    unsigned long int label = 0;
    char temp[MAX_INTERNAL_NAME+1];
    
    while (1)
    {
        char *p = strchr(name, PATH_SEPARATOR);
        
        if (p == NULL)
            break;
        
        sprintf(temp, "%ld/", label);
        strncat(temp, name, p - name);
        label = search(fs, temp, get_label_callback, (void *) temp);
        if (!label)
            return 0;
        sprintf(temp, "%ld/D", label);
        label = search(fs, temp, get_label_callback, (void *) temp);
        if (!label)
            return 0;
        name = p+1;
    }
    
    if (prefix)
        *prefix = label;
    if (short_name)
        *short_name = name;
    
    return 1;
}


/**
 * Translate a filesystem path, e.g. dir/file.txt to an internal name 1234/file.txt.
 */
int find_internal_name(FS *fs, const char *name, char *internal_name)
{
    unsigned long int prefix;
    if (!find_dir_label(fs, name, &prefix, &name))
        return 0;
    
    sprintf(internal_name, "%ld/%s", prefix, name);
    return 1;
}
