#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "fs.h"
#include "fs-internal.h"


int is_special(FS *fs, const char *path)
{
    return (COMPARE_FUNCTION(path, SPECIAL_DIR) == 0) ||
        (COMPARE_N_FUNCTION(path, SPECIAL_DIR "/", strlen(SPECIAL_DIR "/")) == 0);
}


int special_is_dir(FS *fs, const char *path)
{
    if (COMPARE_FUNCTION(path, SPECIAL_DIR) == 0)
        return 1;
    if (COMPARE_FUNCTION(path, SPECIAL_DIR "/super") == 0)
        return 1;
    if (COMPARE_FUNCTION(path, SPECIAL_DIR "/tree") == 0)
        return 1;
    if (COMPARE_FUNCTION(path, SPECIAL_DIR "/data") == 0)
        return 1;
    if (COMPARE_N_FUNCTION(path, SPECIAL_DIR "/tree/", strlen(SPECIAL_DIR "/tree/")) == 0 &&
        strchr(path + strlen(SPECIAL_DIR "/tree/"), '/') == NULL)
        return 1;
    return 0;
}


struct list_tree_baton
{
    TRAVERSE_CALLBACK callback;
    void *callback_data;
    unsigned long int last_label;
};


static int list_tree_callback(const char *name, void *data)
{
    struct list_tree_baton *baton = (struct list_tree_baton *) data;
    unsigned long int label;
    char temp[MAX_INTERNAL_NAME+1];

    sscanf(name, "%ld/", &label);
    if (label == baton->last_label)
        return 0;
    
    sprintf(temp, "%ld", label);
    baton->callback(temp, baton->callback_data);
    baton->last_label = label;
    return 0;
}


int special_list(FS *fs, const char *path, TRAVERSE_CALLBACK callback, void *data)
{
    if (COMPARE_FUNCTION(path, "") == 0)
    {
        callback(SPECIAL_DIR, data);
        return 0;
    }
    else if (COMPARE_FUNCTION(path, SPECIAL_DIR) == 0)
    {
        callback("super", data);
        callback("tree", data);
        callback("data", data);
        return 1;
    }
    else if (COMPARE_FUNCTION(path, SPECIAL_DIR "/tree") == 0)
    {
        struct list_tree_baton baton = { callback, data, -1 };
        BLOCK *root_block = get_block(fs, fs->root_location, 1);
        traverse(fs, root_block, "", 0, list_tree_callback, &baton);
        return 1;
    }
    else if (COMPARE_N_FUNCTION(path, SPECIAL_DIR "/tree/", strlen(SPECIAL_DIR "/tree/")) == 0)
    {
        BLOCK *root_block = get_block(fs, fs->root_location, 1);
        char prefix[MAX_INTERNAL_NAME+1];
        unsigned long int label;
        sscanf(path+strlen(SPECIAL_DIR "/tree/"), "%ld/", &label);
        sprintf(prefix, "%ld/", label);
        //TODO error on no items with prefix
        traverse(fs, root_block, prefix, strlen(prefix), callback, data);
        return 1;
    }
    
    return 0;
}


FH *special_open_file(FS *fs, const char *path)
{
    FH *fh;

    if (COMPARE_N_FUNCTION(path, SPECIAL_DIR "/tree/", strlen(SPECIAL_DIR "/tree/")) != 0)
        return NULL;
    
    //todo verify exists
    
    fh = find_free_handle_slot(fs);
    
    set_flag(fh, F_CACHED);
    clear_flag(fh, F_MODIFIED);
    
    set_flag(fh, F_SPECIAL_TREE);
    strcpy(fh->name, path + strlen(SPECIAL_DIR "/tree/"));
    
    return fh;
}


int special_get_size(FS *fs, const char *path)
{
    char temp[100];
    unsigned long int d;
    
    if (COMPARE_N_FUNCTION(path, SPECIAL_DIR "/tree/", strlen(SPECIAL_DIR "/tree/")) != 0)
        return 0;
    
    fetch_key(fs, path + strlen(SPECIAL_DIR "/tree/"), NULL, &d);
    
    sprintf(temp, "%ld\n", d);
    
    return strlen(temp);
}


int special_read_data(FS *fs, FH *fh, int pos, char *data, int len)
{
    char temp[100];
    unsigned long int d;
    
    if (!(fh->flags & F_SPECIAL_TREE))
        return 0;
    
    fetch_key(fs, fh->name, NULL, &d);
    
    sprintf(temp, "%ld\n", d);
    
    len = MAX2(MIN2(len, strlen(temp) - pos), 0);
    memmove(data, temp+pos, len);
    
    return len;
}


void special_close_file(FS *fs, FH *fh)
{
    clear_flag(fh, F_CACHED);
}
