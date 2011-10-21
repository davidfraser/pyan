/*
 * hash.c - Implementation of functions for manipulating the HASH structure.
 *
 * Copyright (C) 2003, Edmund Horner.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

static unsigned int hash_func(void *key, int key_len, int max, KEY_TYPE key_type)
{
    int i;
    unsigned int h = 0;
    
    if (key_type == key_type_direct)
    {
        for (i = 0; i < 4; i++)
        {
            h = h * 67 + ((char *) &key)[i] - 113;
        }    
    }
    else
    {
        for (i = 0; i < key_len; i++)
        {
            h = h * 67 + ((char *) key)[i] - 113;
        }    
    }
    
    return h % max;
}

static void enlarge_hash(HASH *hash)
{
    int old_max = hash->max;
    HASH_ENTRY *old_entries = hash->entries;
    int i;

    hash->max = hash->max * 2 + 1;
    hash->num = 0;
    hash->entries = malloc(sizeof (HASH_ENTRY) * hash->max);
    
    for (i = 0; i < hash->max; i++)
    {
        hash->entries[i].key = NULL;
        hash->entries[i].key_len = 0;
    }
    
    for (i = 0; i < old_max; i++)
    {
        if (old_entries[i].key)
        {
            add_to_hash(hash, old_entries[i].key, old_entries[i].key_len, old_entries[i].data);
            
            if (hash->key_type == key_type_copyable)
            {
                free(old_entries[i].key);
            }
        }
    }

    free(old_entries);
}

static int different_keys(KEY_TYPE key_type, void *key1, void *key2, int key_len)
{
    if (key_type == key_type_direct && key1 != key2)
        return 1;
    
    return memcmp(key1, key2, key_len);
}

HASH *create_hash(int max, KEY_TYPE key_type)
{
    int i;
    HASH *hash = malloc (sizeof (HASH));
    
    hash->max = max;
    hash->num = 0;
    hash->key_type = key_type;
    
    hash->entries = malloc(sizeof (HASH_ENTRY) * max);
    
    for (i = 0; i < hash->max; i++)
    {
        hash->entries[i].key = NULL;
        hash->entries[i].key_len = 0;
    }
    
    return hash;
}

void destroy_hash(HASH *hash)
{
    if (hash->key_type == key_type_copyable)
    {
        int i;
        
        for (i = 0; i < hash->max; i++)
        {
            if (hash->entries[i].key)
            {
                free(hash->entries[i].key);
            }
        }
    }
    
    free(hash->entries);
    
    free(hash);
}

HASH_ENTRY *find_in_hash(HASH *hash, void *key, int key_len)
{
    int h = hash_func(key, key_len, hash->max, hash->key_type);
    
    int start = h;
    
    while ((hash->entries[h].key == NULL && hash->entries[h].key_len == -1)
        || (hash->entries[h].key != NULL
            && (hash->entries[h].key_len != key_len
                || (hash->key_type == key_type_direct ? key != hash->entries[h].key : memcmp(key, hash->entries[h].key, key_len)))))
    {
        h = (h+1) % hash->max;
        
        if (h == start)
            return NULL;
    }
    
    if (hash->entries[h].key != NULL)
        return &hash->entries[h];
    
    return NULL;
}

void *get_from_hash(HASH *hash, void *key, int key_len)
{
    HASH_ENTRY *he = find_in_hash(hash, key, key_len);
    if (!he)
        return NULL;
    return he->data;
}

int add_to_hash(HASH *hash, void *key, int key_len, void *data)
{
    unsigned int h;
    unsigned int start;
    
    remove_from_hash(hash, key, key_len);
    
    if (hash->num >= 0.75*hash->max)
    {
        enlarge_hash(hash);
    }
    
    h = hash_func(key, key_len, hash->max, hash->key_type);
    start = h;
    
    while (hash->entries[h].key != NULL)
    {
        h = (h+1) % hash->max;
        
        if (h == start)
            return 0;
    }
    
    if (hash->key_type == key_type_copyable)
    {
        hash->entries[h].key = malloc(key_len+1);
        memcpy(hash->entries[h].key, key, key_len);
        ((char *) hash->entries[h].key)[key_len] = 0;
    }
    else
    {
        hash->entries[h].key = key;
    }
    hash->entries[h].key_len = key_len;
    hash->entries[h].data = data;
    
    hash->num++;
    
    return 1;
}

int remove_from_hash(HASH *hash, void *key, int key_len)
{
    int h = hash_func(key, key_len, hash->max, hash->key_type);
    
    int start = h;
    
    while ((hash->entries[h].key == NULL && hash->entries[h].key_len == -1)
        || hash->entries[h].key_len != key_len || different_keys(hash->key_type, key, hash->entries[h].key, key_len))
    {
        h = (h+1) % hash->max;
        
        if (h == start)
            return 0;
    }
    
    if (hash->key_type == key_type_copyable)
    {
        free(hash->entries[h].key);
    }
    
    hash->entries[h].key = NULL;
    hash->entries[h].key_len = -1;
    
    hash->num--;
    
    return 1;
}

int walk_hash(HASH *hash, int (* func)(HASH_ENTRY *he, void *data), void *data)
{
    int i;
    
    for (i = 0; i < hash->max; i++)
    {
        if (hash->entries[i].key)
        {
            int rv = func(&hash->entries[i], data);
            
            if (rv)
                return rv;
        }
    }
    
    return 0;
}

void hash_iterator(HASH *hash, HASH_ITERATOR *iter)
{
    iter->hash = hash;
    iter->pos = -1;
    hash_iterator_next(iter);
}

int hash_iterator_valid(HASH_ITERATOR *iter)
{
    return iter->hash && iter->pos < iter->hash->max;
}

HASH_ENTRY *hash_iterator_next(HASH_ITERATOR *iter)
{
    do
    {
        iter->pos++;
        if (!hash_iterator_valid(iter))
            return NULL;
    }
    while (iter->hash->entries[iter->pos].key == NULL);
    
    iter->entry = &iter->hash->entries[iter->pos];
    return iter->entry;
}
