#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>


int rt_malloc(int size)
{
    int *ptr = malloc((size_t) (size+4));
    *ptr = size;
    //fprintf(stderr, "allocated %d bytes at %d\n", size, (int) ptr+4);
    return (int) ptr+4;
}


int rt_malloc_exec(int size)
{
    int ptr = rt_malloc(size);
    
    int ptr_adj = ptr & 0xFFFFF000;
    size_t size_adj = (ptr + size + 4095) & 0xFFFFF000 - ptr_adj;
    if (mprotect(ptr_adj, size_adj, PROT_READ | PROT_WRITE | PROT_EXEC))
        fprintf(stderr, "Unable to set memory permissions! (ptr_adj = %d, size_adj = %d, errno = %d)\n", ptr_adj, size_adj, errno);
    return ptr;
}


void rt_free(int ptr)
{
    int *p = (int *) ptr;
    int size = p[-1];
    //fprintf(stderr, "freed %d bytes at %d\n", size, ptr);
    free(p - 4);
}
