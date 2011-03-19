public int gcd1(int a, int b)
{
    if (a == 0)
        return b;
    
    while (b != 0)
    {
        if (a > b)
            a = a - b;
        else
            b = b - a;
    }
    return a;
}


public int gcd2(int a, int b)
{
    if (b == 0)
        return a;
    else
    {
        int q;
        int r;
        q = a / b;
        r = a - q * b;
        return gcd2(b, r);
    }
}
