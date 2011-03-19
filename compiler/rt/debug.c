#include <stdio.h>

extern int stub_size;

void print_closure(int *ptr)
{
    int i;
    int size = *(ptr + stub_size/4);
    
    printf("Closure at %d/%p\n", ptr, ptr);
    printf("    Size %d\n", size);
    printf("    Data");
    for (i = 1; i <= size/4; i++)
        printf(" %d/%p", *(ptr + stub_size/4 + i), *(ptr + stub_size/4 + i));
    printf("\n");
    printf("    Function %p\n", *(ptr + stub_size/4 + i));
}
