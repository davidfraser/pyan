#include <stdio.h>

extern int make_closure(int size, int x1, int x2, int ptr);


int cpsfac_inner(int (* k)(int), int n, int x)
{
    return k(n*x);
}


int cpsfac(int x, int (* k)(int))
{
    if (x == 0)
        return k(1);
    else
    {
        int (* new_k) = make_closure(8, k, x, cpsfac_inner);
        return cpsfac(x - 1, new_k);
    }
}


int ret(int x)
{
    return x;
}


int fac(int x)
{
    return cpsfac(x, ret);
}


void main(void)
{
    printf("main, fac(5) = %d\n", fac(5));
}
