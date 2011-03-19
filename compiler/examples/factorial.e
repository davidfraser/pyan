/*
 * This is an example source file for the compiler.
 */

public int factorial1(int x)
{
    if (x <= 1)
    {
        return 1;
    }
    else
    {
        return x * factorial1(x - 1);
    }
}

public int factorial2(int x)
{
    int i;
    int f;
    
    f = 1;
    i = 2;
    while (i <= x)
    {
        f = f * i;
        i = i + 1;
    }
    return f;
}

int factorial3_real(int x, int m)
{
    if (x <= 1)
    {
        return m;
    }
    else
    {
        int old_x;  //TODO needed because tuple assignment "(x, m) = (x-1, x*m)" turns into "x = x-1; m = x*m" which is wrong
        old_x = x;
        return factorial3_real(x - 1, old_x * m);
    }
}

public int factorial3(int x)
{
    return factorial3_real(x, 1);
}

int factorial4_real(int x, (int -> int) c)
{
    if (x <= 1)
    {
        return c(x);
    }
    else
    {
        int old_x;
        old_x = x;
        (int -> int) old_c;
        old_c = c;
        return factorial4_real(x - 1, int lambda(int y) {
            return y * old_c(old_x);
        });
    }
}

public int factorial4(int x)
{
    return factorial4_real(x, int lambda(int x) { return x; });
}
