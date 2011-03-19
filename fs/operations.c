#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "fs.h"
#include "fs-internal.h"


static void flush_fh(FS *fs, FH *fh)
{
    add_labelled_key(fs, fh->label, "A", K_ATTRIBUTE, time(NULL));
    if (fh->flags & F_MODIFIED)
    {
        add_labelled_key(fs, fh->label, "S", K_ATTRIBUTE, fh->size);
        add_labelled_key(fs, fh->label, "M", K_ATTRIBUTE, time(NULL));
    }
}


int is_dir(FS *fs, const char *name)
{
    if (is_special(fs, name))
        return special_is_dir(fs, name);
    
    if (!file_exists(fs, name))
        return 0;
    
    return get_type(fs, name) == K_DIRECTORY;
}


int get_file_size(FS *fs, const char *name)
{
    if (is_special(fs, name))
        return special_get_size(fs, name);
    
    if (!file_exists(fs, name))
        return 0;
    
    return get_attribute(fs, name, "S");
}


unsigned long int get_create_time(FS *fs, const char *name)
{
    if (!file_exists(fs, name))
        return 0;
    
    return get_attribute(fs, name, "C");
}


unsigned long int get_access_time(FS *fs, const char *name)
{
    if (!file_exists(fs, name))
        return 0;
    
    return get_attribute(fs, name, "A");
}


unsigned long int get_modify_time(FS *fs, const char *name)
{
    if (!file_exists(fs, name))
        return 0;
    
    return get_attribute(fs, name, "M");
}


unsigned long int set_access_time(FS *fs, const char *name, unsigned long int t)
{
    if (!file_exists(fs, name))
        return 0;
    
    return set_attribute(fs, name, "A", K_ATTRIBUTE, t);
}


unsigned long int set_modify_time(FS *fs, const char *name, unsigned long int t)
{
    if (!file_exists(fs, name))
        return 0;
    
    return set_attribute(fs, name, "M", K_ATTRIBUTE, t);
}


int fs_mkdir(FS *fs, const char *name)
{
    unsigned long int dir_label;
    unsigned long int label;

    if (file_exists(fs, name))
        return 0;
    
    find_dir_label(fs, name, &dir_label, &name);
    label = create_directory(fs);
    create_link(fs, dir_label, name, label, K_DIRECTORY);
    
    return 1;
}


static int check_emptiness_callback(const char *name, void *baton)
{
    int *has_files = (int *) baton;
    *has_files = 1;
    
    return 1;
}


int fs_rmdir(FS *fs, const char *name)
{
    int has_files = 0;
    unsigned long int dir_label;
    
    /* Check that it's empty. */
    list_fs(fs, name, check_emptiness_callback, &has_files);
    if (has_files)
        return 0;

    if (!find_dir_label(fs, name, &dir_label, &name))
        return 0;
    
    return delete_link(fs, dir_label, name);
}


FH *fs_open_file(FS *fs, const char *filename)
{
    FH *fh = find_free_handle_slot(fs);
    const char *name;
    unsigned long int dir_label;

    if (is_special(fs, filename))
        return special_open_file(fs, filename);
    
    set_flag(fh, F_CACHED);
    clear_flag(fh, F_MODIFIED);
    clear_flag(fh, F_SPECIAL_TREE);
    
    find_dir_label(fs, filename, &dir_label, &name);
    if (!file_exists(fs, filename))
    {
        fh->label = create_file(fs);
        create_link(fs, dir_label, name, fh->label, K_FILE);
    }
    else
    {
        fetch_labelled_key(fs, dir_label, name, NULL, &fh->label);
    }
    
    fetch_labelled_key(fs, fh->label, "S", NULL, &fh->size);
    
    return fh;
}


int fs_link(FS *fs, const char *from_path, const char *to_path)
{
    const char *name;
    unsigned long int dir_label;
    unsigned long int label;
    KEY_TYPE type;
    
    if (is_special(fs, from_path) || is_special(fs, to_path))
        return 0;
    
    find_dir_label(fs, from_path, &dir_label, &name);
    fetch_labelled_key(fs, dir_label, name, &type, &label);
    
    find_dir_label(fs, to_path, &dir_label, &name);
    create_link(fs, dir_label, name, label, type);
    
    return 1;
}


int read_data(FS *fs, FH *fh, int pos, char *data, int len)
{
    int total_read;
    
    if (fh->flags & F_SPECIAL_TREE)
        return special_read_data(fs, fh, pos, data, len);
    
    if (len == 0)
        return 0;
    
    total_read = 0;
    while (len > 0 && pos < fh->size)
    {
        int block_num = POS_2_BLOCK(pos, fs->block_size);
        int block_pos = pos - block_num * fs->block_size;
        int read_len = MIN2(fh->size, MIN2(len, fs->block_size - block_pos));
        BLOCK *b = get_file_data_block(fs, fh, block_num, 0);
        
        memmove(data, b->buffer + block_pos, read_len);
        pos += read_len;
        data += read_len;
        len -= read_len;
        total_read += read_len;
    }

    return total_read;
}


int write_data(FS *fs, FH *fh, int pos, const char *data, int len)
{
    size_t new_file_size = MAX2(fh->size, pos+len);
    size_t total_written;
    unsigned long int predecessor = 0;
    
    if (fh->flags & F_SPECIAL_TREE)
        return 0;
    
    if (len == 0)
        return 0;
    
    total_written = 0;
    while (len > 0)
    {
        int block_num = POS_2_BLOCK(pos, fs->block_size);
        int block_pos = pos - block_num * fs->block_size;
        size_t write_len = MIN2(len, fs->block_size - block_pos);
        BLOCK *b = get_file_data_block(fs, fh, block_num, predecessor);
        
        memmove(b->buffer + block_pos, data, write_len);
        pos += write_len;
        data += write_len;
        len -= write_len;
        total_written += write_len;
        set_flag(b, F_DIRTY);
        predecessor = b->location;
    }
    
    fh->size = new_file_size;

    set_flag(fh, F_MODIFIED);
    return total_written;
}


void fs_close_file(FS *fs, FH *fh)
{
    if (fh->flags & F_SPECIAL_TREE)
    {
        special_close_file(fs, fh);
        return;
    }
    
    flush_fh(fs, fh);
    clear_flag(fh, F_CACHED);
}


int fs_delete_file(FS *fs, const char *filename)
{
    unsigned long int dir_label;

    if (!find_dir_label(fs, filename, &dir_label, &filename))
        return 0;
    
    return delete_link(fs, dir_label, filename);
}


int list_fs(FS *fs, const char *dirname, TRAVERSE_CALLBACK callback, void *baton)
{
    char temp[MAX_INTERNAL_NAME+1];
    char prefix[MAX_INTERNAL_NAME+1];
    char *p;
    BLOCK *root_block;
    
    if (special_list(fs, dirname, callback, baton))
        return 1;
    
    if (dirname[0] == 0)
    {
        strcpy(prefix, "0/");
    }
    else
    {    
        sprintf(temp, "%s/", dirname);
        //printf("T %s\n", temp);
        if (!find_internal_name(fs, temp, prefix))
            return 0;
    }
    
    p = strchr(prefix, '/');
    if (p)
        *(p+1) = 0;
    
    //printf("P %s\n", prefix);
    root_block = get_block(fs, fs->root_location, 1);
    traverse(fs, root_block, prefix, strlen(prefix), callback, baton);
    
    return 1;
}


static int file_exists_callback(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data)
{
    if (key->pos >= 0 && !COMPARE_FUNCTION(key->name, (char *) data))
        return 1;
    return 0;
}


int file_exists(FS *fs, const char *name)
{
    char internal_name[MAX_INTERNAL_NAME+1];

    if (is_special(fs, name))
        return 1;
    
    if (!find_internal_name(fs, name, internal_name))
        return 0;
    
    return search(fs, internal_name, file_exists_callback, internal_name);
}


int fs_truncate(FS *fs, FH *fh, int new_size)
{
    reduce_file(fs, fh->label, fh->size, new_size);
    
    fh->size = new_size;
    set_flag(fh, F_MODIFIED);
    set_flag(fh, F_DIRTY);

    return 1;
}
