public int max(int a, int b)
{
    if (a > b)
        return a;
    else
        return b;
}


public int min(int a, int b)
{
    return a + b - max(a, b);
}
