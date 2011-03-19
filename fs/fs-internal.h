#ifndef FS_INTERNAL_H
#define FS_INTERNAL_H


#include <stdio.h>

#include "fs.h"


extern int total_pins;

extern int cache_lookups;
extern int cache_lookup_cost;

extern int key_iterations;
extern int key_iteration_cost;


#define DEFAULT_BLOCK_SIZE 4096
#define MIN_BLOCK_SIZE 256
#define DEFAULT_TOTAL_CACHE_SIZE 1024*1024
#define DEFAULT_CACHE_SIZE 1024
#define MIN_CACHE_SIZE 16
#define MAX_CACHE_SIZE 4096
#define DEFAULT_CACHE_HASH_SIZE 1024
#define DEFAULT_FH_CACHE_SIZE 1024
#define MAX_INTERNAL_NAME 127
#define MAX_NAME (MAX_INTERNAL_NAME-15)
#define MAX_PINS 100
#define RESIZE_THRESHOLD 0.67
#define SPECIAL_DIR ".FS"

#ifdef WIN32
    #define COMPARE_FUNCTION stricmp
    #define COMPARE_N_FUNCTION strnicmp
#else
    #define COMPARE_FUNCTION strcasecmp
    #define COMPARE_N_FUNCTION strncasecmp
#endif

#define PATH_SEPARATOR '/'


#define ROUND_UP(p, n)  ((((p)+(n)-1) / (n)) * (n))
#define ALIGN_POINTER(p, n)  (char *) (ROUND_UP(((size_t) p), (n)))
#define MIN2(a, b) ((a) < (b) ? (a) : (b))
#define MAX2(a, b) ((a) > (b) ? (a) : (b))

#define SIZE_2_BLOCKS(s, bs) (((s) + (bs) - 1) / (bs))
#define POS_2_BLOCK(s, bs) ((s) / (bs))

#define set_flag(b, f) (b)->flags |= (f)
#define clear_flag(b, f) (b)->flags &= (~f)


typedef unsigned long int LOCATION;


enum {
    F_CACHED = 0x01,
    F_DIRTY = 0x02,
    F_VALID = 0x04,
    F_CLONE = 0x08,
    F_MODIFIED = 0x10,
    F_SPECIAL_TREE = 0x20
};


typedef enum {
    B_SUPER,
    B_DATA,
    B_TREE,
    B_FREE,
    B_BITMAP,
    NUM_BLOCK_TYPES
} BLOCK_TYPE;


typedef enum {
    K_INVALID,
    K_NODE,
    K_DIRECTORY,
    K_FILE,
    K_INLINE,
    K_ATTRIBUTE,
    K_MIN_KEY,
    K_MAX_KEY
} KEY_TYPE;


typedef enum {
    E_NONE,
    E_SPLIT,
    E_SHRINK
} EFFECT_TYPE;


typedef struct BLOCK
{
    BLOCK_TYPE type;
    int flags;
    int pins;
    struct BLOCK *next, *prev;
    struct BLOCK *hash_next;
    
    LOCATION location;
    char *buffer;
} BLOCK;


typedef struct INDEX
{
    BLOCK *block;
    char *strings;
    unsigned char *types;
    unsigned short int *offsets;
    unsigned long int *data;
} INDEX;


typedef struct KEY
{
    KEY_TYPE type;
    LOCATION pointer;
    int pos;
    char *name;
} KEY;


struct FH
{
    int flags;
    char name[MAX_INTERNAL_NAME + 1];
    unsigned long int size;
    unsigned long int num_blocks;
    unsigned long int label;
};


struct FS
{
    char *filename;
    FILE *f;
    unsigned long int num_blocks;
    unsigned long int blocks_written;
    unsigned long int block_size;
    LOCATION root_location;
    int next_label;
    unsigned long int max_bitmap_pointers;
    unsigned long int num_bitmap_pointers;
    unsigned long int bitmap_size;
    
    BLOCK *superblock;
    
    int cache_size;
    BLOCK *cache;
    char *buffer;
    BLOCK *cache_head, *cache_tail;
    int cache_hash_size;
    BLOCK **cache_hash;
    
    int fh_cache_size;
    FH *fh_cache;
    
    unsigned long int *bitmaps;
};


struct block_header
{
    unsigned long int version;
    unsigned long int location;
    unsigned long int type;
};


struct superblock_header
{
    struct block_header h;
    unsigned long int block_size;
    unsigned long int root_location;
    unsigned long int next_label;
    unsigned long int block_counts[NUM_BLOCK_TYPES];
    unsigned long int max_bitmap_pointers;
    unsigned long int num_bitmap_pointers;
    unsigned long int bitmap_size;
};


struct tree_block_header {
    struct block_header h;
    unsigned short int height;
    unsigned short int num_keys;
    unsigned short int string_size;
};


struct bitmap_block_header
{
    struct block_header h;
    unsigned long int num_bits;
    unsigned long int used_bits;
};


typedef struct EFFECT
{
    EFFECT_TYPE type;
    BLOCK *new_blocks[2];
    int pending_pins;
    char new_name[MAX_INTERNAL_NAME+1];
} EFFECT;


typedef int (* SEARCH_CALLBACK)(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data);


typedef enum {
    O_FETCH,
    O_INSERT,
    O_REPLACE,
    O_DELETE
} KEY_OPERATION_TYPE;


struct key_operation_baton
{
    KEY_OPERATION_TYPE operation;
    const char *name;
    KEY_TYPE type;
    unsigned long int data;
};


/* General FS functions. */
extern int is_watched(LOCATION location);
extern void error(char *fmt, ...);
extern void print_key(KEY *key);
extern void print_block(FS *fs, BLOCK *block);
extern void print_tree(FS *fs, BLOCK *block, int indent);
extern FH *find_free_handle_slot(FS *fs);

/* Block functions. */
extern void pin(BLOCK *b);
extern void unpin(BLOCK *b);

extern void flush_block(FS *fs, BLOCK *block);
extern BLOCK *allocate_block(FS *fs, BLOCK_TYPE type, LOCATION target);
extern void deallocate_block(FS *fs, BLOCK_TYPE type, LOCATION location);
extern BLOCK *get_block(FS *fs, LOCATION location, int parse);

extern BLOCK *clone_block(FS *fs, BLOCK *block);
extern void unclone_block(FS *fs, BLOCK *block);

/* Tree functions. */
extern void print_keys(FS *fs, BLOCK *block);
extern void get_index(FS *fs, BLOCK *block, INDEX *index);
extern void insert_key(FS *fs, BLOCK *block, const char *name, KEY_TYPE type, LOCATION pointer, EFFECT *effect);
extern void update_key(FS *fs, BLOCK *block, const char *name, KEY_TYPE type, LOCATION data);
extern void delete_key(FS *fs, BLOCK *block, const char *name, EFFECT *effect);
extern void replace_key(FS *fs, BLOCK *block, const char *old_name, const char *name, KEY_TYPE type, LOCATION data, EFFECT *effect);
extern int search(FS *fs, const char *name, SEARCH_CALLBACK callback, void *baton);
extern int traverse(FS *fs, BLOCK *block, const char *prefix, size_t prefix_len, TRAVERSE_CALLBACK callback, void *baton);
extern int key_operation_callback(FS *fs, BLOCK *block, KEY *key, EFFECT *effect, void *data);
int fetch_key(FS *fs, const char *name, KEY_TYPE *type, unsigned long int *data);

/* File functions. */
extern void add_labelled_key(FS *fs, unsigned long int label, const char *name, KEY_TYPE type, unsigned long int data);
extern void delete_labelled_key(FS *fs, unsigned long int label, const char *name);
extern int fetch_labelled_key(FS *fs, unsigned long int label, const char *name, KEY_TYPE *type, unsigned long int *data);
extern KEY_TYPE get_type(FS *fs, const char *name);
extern int get_attribute(FS *fs, const char *name, const char *attribute);
extern int set_attribute(FS *fs, const char *name, const char *attribute, KEY_TYPE type, unsigned long int data);
extern unsigned long int create_file(FS *fs);
extern unsigned long int create_directory(FS *fs);
extern void create_link(FS *fs, unsigned long int dir_label, const char *name, unsigned long int label, KEY_TYPE type);
extern BLOCK *get_file_data_block(FS *fs, FH *fh, unsigned long int num, unsigned long int predecessor);
extern void reduce_file(FS *fs, unsigned long int label, unsigned long int size, unsigned long int new_size);
extern int delete_link(FS *fs, unsigned long int dir_label, const char *name);
extern int find_dir_label(FS *fs, const char *name, unsigned long int *prefix, const char **short_name);
extern int find_internal_name(FS *fs, const char *name, char *internal_name);

/* File functions for special files. */
extern int is_special(FS *fs, const char *path);
extern int special_is_dir(FS *fs, const char *path);
extern int special_list(FS *fs, const char *dirname, TRAVERSE_CALLBACK callback, void *baton);
extern FH *special_open_file(FS *fs, const char *path);
extern int special_get_size(FS *fs, const char *path);
extern int special_read_data(FS *fs, FH *fh, int pos, char *data, int len);
extern void special_close_file(FS *fs, FH *fh);

#endif
