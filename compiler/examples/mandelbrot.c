#include <stdio.h>

void output(int c)
{
    if (c < 0 || c > 255)
        fprintf(stderr, "oi, c = %d!\n", c);
    
    putc(c, stdout);
}


void debug(int x)
{
    fprintf(stderr, "%d ", x);
}


int main(void)
{
    fprintf(stderr, "fixabs(5) = %d\n", fixabs(5));
    fprintf(stderr, "fixabs(-5) = %d\n", fixabs(-5));
    fprintf(stderr, "fixplus(7000000, 9000000) = %d\n", fixplus(7000000, 9000000));
    fprintf(stderr, "fixminus(7000000, 9000000) = %d\n", fixminus(7000000, 9000000));
    fprintf(stderr, "fixtimes(13000000, 3000000) = %d\n", fixtimes(13000000, 3000000));
    fprintf(stderr, "fixdiv(13000000, 3000000) = %d\n", fixdiv(13000000, 3000000));
    fprintf(stderr, "fixsqrt(1313698) = %d\n", fixsqrt(1313698));
    fractal();
    return 0;
}
