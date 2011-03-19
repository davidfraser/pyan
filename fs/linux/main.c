#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "../fs.h"


#define MAX_ACTIVE_FS 10

static FS *active_fs[MAX_ACTIVE_FS];
static int num_active_fs;


static int op_getattr(const char *path, struct stat *stbuf)
{
    FS *fs = fuse_get_context()->private_data;
    int s;
    
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    if (!file_exists(fs, path+1))
        return -ENOENT;
    
    if (is_dir(fs, path+1))
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    s = get_file_size(fs, path+1);
    
    stbuf->st_ctime = get_create_time(fs, path+1);
    stbuf->st_atime = get_access_time(fs, path+1);
    stbuf->st_mtime = get_modify_time(fs, path+1);
    
    stbuf->st_mode = S_IFREG | 0666;
    stbuf->st_nlink = 1;
    stbuf->st_size = s;
    return 0;
}


struct readdir_baton {
    void *buf;
    fuse_fill_dir_t filler;
};


static int readdir_callback(const char *name, void *data)
{
    struct readdir_baton *baton = (struct readdir_baton *) data;

    baton->filler(baton->buf, name, NULL, 0);

    return 0;
}


static int op_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    struct readdir_baton baton = { buf, filler };
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    if (!list_fs(fs, path+1, readdir_callback, &baton))
        return -ENOENT;
    return 0;
}


static int op_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    FH *fh;
    if (file_exists(fs, path+1))
    {
        return -EEXIST;
    }
    
    fh = fs_open_file(fs, path+1);
    fi->fh = (uint64_t) fh;
    
    return 0;
}


static int op_link(const char *from_path, const char *to_path)
{
    FS *fs = fuse_get_context()->private_data;
    if (!file_exists(fs, from_path+1))
    {
        return -ENOENT;
    }
    if (file_exists(fs, to_path+1))
    {
        return -EEXIST;
    }
    
    fs_link(fs, from_path+1, to_path+1);
    
    return 0;
}


static int op_open(const char *path, struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    FH *fh;
    if (!file_exists(fs, path+1))
    {
        return -ENOENT;
    }
    
    fh = fs_open_file(fs, path+1);
    if (fh == NULL)
        return -EPERM;
    fi->fh = (uint64_t) fh;
    
    return 0;
}


static int op_release(const char *path, struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    FH *fh = (FH *) fi->fh;
    
    fs_close_file(fs, fh);
    
    return 0;
}


static int op_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    FH *fh = (FH *) fi->fh;
    
    return read_data(fs, fh, offset, buf, size);
}


static int op_write(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    FH *fh = (FH *) fi->fh;
    
    return write_data(fs, fh, offset, buf, size);
}


static int op_ftruncate(const char *path, off_t new_size, struct fuse_file_info *fi)
{
    FS *fs = fuse_get_context()->private_data;
    FH *fh = (FH *) fi->fh;
    
    fs_truncate(fs, fh, new_size);
    
    return 0;
}


static int op_truncate(const char *path, off_t new_size)
{
    FS *fs = fuse_get_context()->private_data;
    
    if (!file_exists(fs, path+1))
        return -ENOENT;
    
    FH *fh = fs_open_file(fs, path+1);
    fs_truncate(fs, fh, new_size);
    fs_close_file(fs, fh);
    
    return 0;
}


static int op_unlink(const char *path)
{
    FS *fs = fuse_get_context()->private_data;
    
    if (!fs_delete_file(fs, path+1))
        return -ENOENT;
    
    return 0;
}


static int op_rename(const char *from_path, const char *to_path)
{
    FS *fs = fuse_get_context()->private_data;
    
    if (!file_exists(fs, from_path+1))
        return -ENOENT;
    if (file_exists(fs, to_path+1))
        return -EEXIST;
    
    fs_link(fs, from_path+1, to_path+1);
    fs_delete_file(fs, from_path+1);
    
    return 0;
}


static void *op_init(struct fuse_conn_info *conn)
{
    int block_size = 4096;
    FS *fs = open_fs("/home/edmund/test.fs");
    if (fs == NULL)
        fs = create_fs("/home/edmund/test.fs", block_size);

    if (num_active_fs < MAX_ACTIVE_FS)
    {
        active_fs[num_active_fs] = fs;
        num_active_fs++;
    }
    
    return fs;
}


static int op_mkdir(const char *path)
{
    FS *fs = fuse_get_context()->private_data;
    
    if (!fs_mkdir(fs, path+1))
        return -EEXIST;

    return 0;
}


static int op_rmdir(const char *path)
{
    FS *fs = fuse_get_context()->private_data;
    
    if (!fs_rmdir(fs, path+1))
        return -ENOENT;

    return 0;
}


static int op_utimens(const char *path, const struct timespec tv[2])
{
    FS *fs = fuse_get_context()->private_data;
    
    if (!file_exists(fs, path+1))
        return -ENOENT;
    
    set_access_time(fs, path+1, tv[0].tv_sec);
    set_modify_time(fs, path+1, tv[1].tv_sec);
    
    return 0;
}


static void op_destroy(void *data)
{
    int i;
    FS *fs = (FS *) data;
    close_fs(fs);
    
    for (i = 0; i < num_active_fs; i++)
        if (active_fs[i] == fs)
        {
            active_fs[i] = NULL;
            active_fs[i] = active_fs[num_active_fs-1];
            num_active_fs--;
            break;
        }
}

static struct fuse_operations op_vtable = {
    .init = op_init,
    .destroy = op_destroy,
    .getattr = op_getattr,
    .readdir = op_readdir,
    .create = op_create,
    .link = op_link,
    .open = op_open,
    .release = op_release,
    .read = op_read,
    .write = op_write,
    .unlink = op_unlink,
    .rename = op_rename,
    .mkdir = op_mkdir,
    .rmdir = op_rmdir,
    .ftruncate = op_ftruncate,
    .truncate = op_truncate,
    .utimens = op_utimens
};


static void catch_alarm(int sig)
{
    int i;
    
    for (i = 0; i < num_active_fs; i++)
        if (active_fs[i])
        {
            flush_fs(active_fs[i]);
        }
    
    alarm(2);
}


int main(int argc, char *argv[])
{
    int i;

    for (i = 0; i < MAX_ACTIVE_FS; i++)
        active_fs[i] = NULL;
    num_active_fs = 0;
    
    signal(SIGALRM, catch_alarm);
    alarm(2);
    
    return fuse_main(argc, argv, &op_vtable, NULL);
}
