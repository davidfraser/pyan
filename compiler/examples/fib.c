#include <stdio.h>

void print_int(int x)
{
    printf("%d\n", x);
}

int main(int argc, char *argv[])
{
    print_int(argc);
    printf("fib = %d\n", fib());
    return 0;
}
