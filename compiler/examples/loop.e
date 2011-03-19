public int main()
{
    int a;
    int b;
    
    a = 1;
    b = 2;
    
    while (a <= b)
    {
        if (b*b <= a)
        {
            int d;
            if (a <= d && d <= b)
            {
                b = b+b+b+b;
            }
        }
        else
        {
            int c;
            while (c <= a)
            {
                b = b+1;
            }
            while (a +1 <= a - 1)
            {
                a = a * 2;
            }
        }
    }
    
    return a;
}
