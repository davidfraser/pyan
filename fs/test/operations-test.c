#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../fs.h"


#define TEST(n) do { \
    char *msg; \
    printf("Running: %s\n", #n); \
    msg = n(); \
    if (msg) { printf("FAILED in %s\n", msg); failures++; exit(1); } \
    else { printf("PASSED\n"); passes++; } \
} while (0)


static int block_size;


static char *test_create(void)
{
    FS *fs;
   
    remove("test1");
    fs = create_fs("test1", block_size);
    if (!fs)
        return "open_fs";
   
    close_fs(fs);
   
    return NULL;
}


struct count_baton
{
    int file_count;
    int dir_count;
};


static int count_file(const char *name, void *data)
{
    struct count_baton *baton = (struct count_baton *) data;
   
    if (name[0] == 'f')
        baton->file_count++;
    else if (name[0] == 's')
        baton->dir_count++;
   
    return 0;
}


#define TEST_SIZE 1000


static char *test_create_file(void)
{
    FS *fs;
    int i, j;
   
    remove("test2");
    fs = create_fs("test2", block_size);
    if (!fs)
        return "open_fs";
   
    for (i = 1; i <= TEST_SIZE; i++)
    {
        FH *fh;
        char temp[100];
        
        //fs_watch_mode = (i == 151);
        sprintf(temp, "fil%d", i);
        fh = fs_open_file(fs, temp);
        fs_watch_mode = 0;
        if (!fh)
            return "assertion (file not opened)";
        
        fs_close_file(fs, fh);
        
        for (j = 1; j <= i; j++)
        {
            char temp2[100];
            int r;
            
            //fs_watch_mode = (i == 151 && j == 14);
            sprintf(temp2, "fil%d", j);
            r = file_exists(fs, temp2);
            fs_watch_mode = 0;
            if (!r) {
                printf("Error finding file '%s' after inserting '%s'\n", temp2, temp);
                return "assertion (file not found)";
            }
        }
        if (i % 100000 == 0)
        {
            printf("%d ", i);
            fflush(stdout);
        }
    }
   
    close_fs(fs);
   
    return NULL;
}


static char *test_create_subdir(void)
{
    FS *fs;
    int i;
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
   
    for (i = 1; i <= TEST_SIZE; i++)
    {
        char temp[100];
       
        sprintf(temp, "subdir%d", i);
        fs_mkdir(fs, temp);
        
        if (!file_exists(fs, temp))
        {
            printf("Error checking existing of subdir '%s'\n", temp);
            return "assertion (subdir not found)";
        }
    }
   
    close_fs(fs);
   
    return NULL;
}


static char *test_create_file_in_subdir(void)
{
    FS *fs;
    FH *fh;
    char temp[100];
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
    
    sprintf(temp, "subdir543/file.txt");
    fh = fs_open_file(fs, temp);
    if (!fh)
        return "assertion (file not opened)";
    
    fs_close_file(fs, fh);
   
    if (!file_exists(fs, temp))
        return "assertion (file not found)";

    close_fs(fs);
   
    return NULL;
}


static char *test_list(void)
{
    FS *fs;
    struct count_baton baton;
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
   
    baton.file_count = 0;
    baton.dir_count = 0;
    list_fs(fs, "", count_file, &baton);
    if (baton.file_count != TEST_SIZE || baton.dir_count != TEST_SIZE)
    {
        printf("Error on count (files was %d, dirs was %d)\n", baton.file_count, baton.dir_count);
        return "assertion";
    }
   
    close_fs(fs);
   
    return NULL;
}


static char *test_list_subdir(void)
{
    FS *fs;
    struct count_baton baton;
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
   
    baton.file_count = 0;
    baton.dir_count = 0;
    list_fs(fs, "subdir543", count_file, &baton);
    if (baton.file_count != 1 || baton.dir_count != 0)
    {
        printf("Error on count (files was %d, dirs was %d)\n", baton.file_count, baton.dir_count);
        return "assertion";
    }
   
    close_fs(fs);
   
    return NULL;
}


static char *test_find(void)
{
    FS *fs;
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
    
    if (!file_exists(fs, "fil962"))
        return "file_exists (not found)";
   
    close_fs(fs);
   
    return NULL;
}


static char *test_delete_all_files(void)
{
    FS *fs;
    int i, j;
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
   
    for (i = 1; i <= TEST_SIZE; i++)
    {
        char temp[100];
        
        sprintf(temp, "fil%d", i);
        if (!fs_delete_file(fs, temp))
        {
            printf("Error deleting file '%s'\n", temp);
            return "assertion (file not deleted)";
        }
        
        if (file_exists(fs, temp))
            return "assertion (file still exists)";
        
        for (j = i+1; j <= TEST_SIZE; j++)
        {
            char temp2[100];
            
            sprintf(temp2, "fil%d", j);
            if (!file_exists(fs, temp2))
            {
                printf("Error finding file '%s' after deleting '%s'\n", temp2, temp);
                return "assertion (file not found)";
            }
        }
        if (i % 100000 == 0)
        {
            printf("%d ", i);
            fflush(stdout);
        }
    }
   
    close_fs(fs);
   
    return NULL;
}


static char *test_remove_all_subdirs(void)
{
    FS *fs;
    int i;
   
    fs = open_fs("test2");
    if (!fs)
        return "open_fs";
   
    for (i = 1; i <= TEST_SIZE; i++)
    {
        char temp[100];
       
        if (i == 543)
            continue;
        
        sprintf(temp, "subdir%d", i);
        fs_rmdir(fs, temp);
        
        if (file_exists(fs, temp))
        {
            printf("Error checking existing of subdir '%s'\n", temp);
            return "assertion (subdir still exists)";
        }
    }
   
    close_fs(fs);
   
    return NULL;
}


static char *test_write_1(void)
{
    FS *fs;
    FH *fh;
    int nw;
   
    remove("test3");
    fs = create_fs("test3", block_size);
    if (!fs)
        return "open_fs";
   
    fh = fs_open_file(fs, "testfile");

    nw = write_data(fs, fh, 0, "?", 1);
    if (nw != 1)
    {
        printf("Error on writing, wrote %d\n", nw);
        return "write_data";
    }

    fs_close_file(fs, fh);

    close_fs(fs);
   
    return NULL;
}


static char *test_read_1(void)
{
    FS *fs;
    FH *fh;
    char buffer[100];
    int nr;
   
    fs = open_fs("test3");
    if (!fs)
        return "fs_open";
    
    fh = fs_open_file(fs, "testfile");
    if (!fh)
        return "open file\n";

    nr = read_data(fs, fh, 0, buffer, 100);
    if (nr != 1)
    {
        printf("Error on reading, read %d\n", nr);
        return "read_data";
    }

    if (buffer[0] != '?')
    {
        printf("Error on reading, read incorrect data (was %d)\n", buffer[0]);
        return "assertion";
    }

    fs_close_file(fs, fh);

    close_fs(fs);
   
    return NULL;
}


static char *test_delete(void)
{
    FS *fs;
   
    fs = open_fs("test3");
    if (!fs)
        return "fs_open";
   
    if (!fs_delete_file(fs, "testfile"))
        return "fs_delete_file";
   
    if (file_exists(fs, "testfile"))
        return "assertion (file still exists)";

    close_fs(fs);
   
    return NULL;
}


static char *test_write_1000000(void)
{
    FS *fs;
    FH *fh;
    int nw;
    int i;
    char buffer[1000];
   
    remove("test4");
    fs = create_fs("test4", block_size);
    if (!fs)
        return "fs_open";
    
    fh = fs_open_file(fs, "testfile2");

    for (i = 0; i < 1000; i++)
    {
        memset(buffer, i % 256, 1000);
        nw = write_data(fs, fh, i*1000, buffer, 1000);
        if (nw != 1000)
        {
            printf("Error on writing, wrote %d\n", nw);
            return "assertion";
        }
    }

    fs_close_file(fs, fh);

    close_fs(fs);
   
    return NULL;
}


static char *test_read_1000000(void)
{
    FS *fs;
    FH *fh;
    char buffer[1000];
    int nr;
    int i;
   
    fs = open_fs("test4");
    if (!fs)
        return "open_fs";
   
    fh = fs_open_file(fs, "testfile2");
    if (!fh)
        return "fs_open_file";
    
    for (i = 0; i < 1000; i++)
    {
        nr = read_data(fs, fh, i*1000, buffer, 1000);
        if (nr != 1000)
        {
            printf("Error on reading, read %d\n", nr);
            return "assertion";
        }

        if ((unsigned char) buffer[i] != i % 256)
        {
            printf("Error on reading, read incorrect data (was %d)\n", buffer[i]);
            return "assertion";
        }
    }

    fs_close_file(fs, fh);

    close_fs(fs);
   
    return NULL;
}


static char *test_reduce_big(void)
{
    FS *fs;
    FH *fh;
   
    fs = open_fs("test4");
    if (!fs)
        return "open_fs";
   
    fh = fs_open_file(fs, "testfile2");
    if (!fh)
        return "fs_open_file";
    
    if (!fs_truncate(fs, fh, 500000))
        return "fs_truncate";

    fs_close_file(fs, fh);

    if (get_file_size(fs, "testfile2") != 500000)
        return "assertion (incorrect size after truncate)";
    
    close_fs(fs);
   
    return NULL;
}


static char *test_write_more(void)
{
    FS *fs;
    FH *fh;
    int nw;
    int i;
    char buffer[1000];
   
    fs = open_fs("test4");
    if (!fs)
        return "fs_open";
    
    fh = fs_open_file(fs, "testfile2");

    for (i = 500; i < 1000; i++)
    {
        memset(buffer, i % 256, 1000);
        nw = write_data(fs, fh, i*1000, buffer, 1000);
        if (nw != 1000)
        {
            printf("Error on writing, wrote %d\n", nw);
            return "assertion";
        }
    }

    fs_close_file(fs, fh);

    close_fs(fs);
   
    return NULL;
}


static char *test_delete_big(void)
{
    FS *fs;
   
    fs = open_fs("test4");
    if (!fs)
        return "fs_open";
   
    if (!fs_delete_file(fs, "testfile2"))
        return "fs_delete_file";
   
    if (file_exists(fs, "testfile"))
        return "assertion (file still exists)";

    close_fs(fs);
   
    return NULL;
}


int test_main(int argc, char *argv[])
{
    int block_sizes[] = { 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
    int passes = 0, failures = 0;
    int i;

    for (i = 0; i < sizeof(block_sizes)/sizeof(int); i++)
    {
        clock_t start = clock();
        clock_t stop;
        
        block_size = block_sizes[i];
        printf("Running tests for block size %d\n", block_size);
        
        TEST(test_create);
       
        TEST(test_create_file);
        TEST(test_create_subdir);
        TEST(test_create_file_in_subdir);
        TEST(test_list);
        TEST(test_list_subdir);
        TEST(test_find);
        TEST(test_delete_all_files);
        TEST(test_remove_all_subdirs);

        TEST(test_write_1);
        TEST(test_read_1);
        TEST(test_delete);
       
        TEST(test_write_1000000);
        TEST(test_read_1000000);
        TEST(test_reduce_big);
        TEST(test_write_more);
        TEST(test_delete_big);
        
        stop = clock();
        printf("Time for tests was %1.6f\n", (double) (stop - start) / CLOCKS_PER_SEC);
    }
    
    if (failures == 0)
        printf("All %d tests passed.\n", passes);
    else
        printf("Passes: %d, Failures: %d\n", passes, failures);
   
    return 0;
}
