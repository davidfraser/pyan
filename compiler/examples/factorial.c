#include <stdio.h>

extern int factorial1(int x);
extern int factorial2(int x);
extern int factorial3(int x);
extern int factorial4(int x);

void debug(int x)
{
    printf("debug: %d\n", x);
}

int main(int argc, char *argv[])
{
    printf("factorial1(5) = %d\n", factorial1(5));
    printf("factorial2(5) = %d\n", factorial2(5));
    printf("factorial3(5) = %d\n", factorial3(5));
    //disabled as its totally broken for now!
    //printf("factorial4(5) = %d\n", factorial4(5));
    return 0;
}
