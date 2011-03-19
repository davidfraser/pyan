(int, int) fib_init()
{
    return (1, 1);
}

(int, int) fib_next(int a, int b)
{
    return (b, a + b);
}

int print_int(int x);

public int fib()
{
    int a;
    int b;
    
    int i;
    
    (a, b) = fib_init();

    i = 0;
    while (i < 10)
    {
        (a, b) = fib_next(a, b);
        print_int(a);
        i = i + 1;
    }
    
    return b;
}
