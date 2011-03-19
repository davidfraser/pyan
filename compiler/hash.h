/*
 * hash.h - Defintion of the HASH structure and functions for manipulating it.
 *
 * Copyright (C) 2003, Edmund Horner.
 */

#ifndef HASH_H
#define HASH_H

#define LARGE_HASHTABLE  65521
#define MEDIUM_HASHTABLE 8191
#define SMALL_HASHTABLE  1021
#define TINY_HASHTABLE   127

typedef enum KEY_TYPE { key_type_direct, key_type_indirect, key_type_copyable } KEY_TYPE;

typedef struct HASH_ENTRY
{
    void *key;
    int key_len;
    
    void *data;
} HASH_ENTRY;

typedef struct HASH
{
    unsigned int max;
    unsigned int num;
    KEY_TYPE key_type;
    
    HASH_ENTRY *entries;
} HASH;

typedef struct HASH_ITERATOR
{
    HASH *hash;
    int pos;
    HASH_ENTRY *entry;
} HASH_ITERATOR;

extern HASH *create_hash(int max, KEY_TYPE key_type);
extern void destroy_hash(HASH *hash);

extern HASH_ENTRY *find_in_hash(HASH *hash, void *key, int key_len);
extern void *get_from_hash(HASH *hash, void *key, int key_len);
extern int add_to_hash(HASH *hash, void *key, int key_len, void *data);
extern int remove_from_hash(HASH *hash, void *key, int key_len);

extern int walk_hash(HASH *hash,
                     int (* func)(HASH_ENTRY *he, void *data), void *data);

extern void hash_iterator(HASH *hash, HASH_ITERATOR *iter);
extern int hash_iterator_valid(HASH_ITERATOR *iter);
extern HASH_ENTRY *hash_iterator_next(HASH_ITERATOR *iter);

#endif
