public int plus1(int x, int p)
{
    if (x <= 0)
        return p;
    else
        return plus1(x - 1, p + 1);
}
