#ifndef FS_H
#define FS_H


typedef struct FS FS;

typedef struct FH FH;

typedef int (* TRAVERSE_CALLBACK)(const char *name, void *baton);

extern int fs_watch_mode;

extern FS *create_fs(const char *filename, int block_size);
extern FS *open_fs(const char *filename);
extern void flush_fs(FS *fs);
extern void close_fs(FS *fs);

extern int fs_mkdir(FS *fs, const char *name);
extern int fs_rmdir(FS *fs, const char *name);

extern FH *fs_open_file(FS *fs, const char *filename);
extern int read_data(FS *fs, FH *fh, int pos, char *data, int len);
extern int write_data(FS *fs, FH *fh, int pos, const char *data, int len);
extern void fs_close_file(FS *fs, FH *fh);
extern int fs_delete_file(FS *fs, const char *filename);

extern int list_fs(FS *fs, const char *dirname, TRAVERSE_CALLBACK callback, void *baton);
extern int file_exists(FS *fs, const char *name);
extern int is_dir(FS *fs, const char *name);
extern int get_file_size(FS *fs, const char *name);
extern unsigned long int get_create_time(FS *fs, const char *name);
extern unsigned long int get_access_time(FS *fs, const char *name);
extern unsigned long int get_modify_time(FS *fs, const char *name);
extern unsigned long int set_access_time(FS *fs, const char *name, unsigned long int t);
extern unsigned long int set_modify_time(FS *fs, const char *name, unsigned long int t);
extern int fs_truncate(FS *fs, FH *fh, int new_size);


#endif
