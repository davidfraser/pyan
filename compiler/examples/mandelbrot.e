public int fixabs(int x)
{
    if (x < 0)
        return -x;
    else
        return x;
}

public int fixplus(int x, int y)
{
    return x + y;
}

public int fixminus(int x, int y)
{
    return x - y;
}

public int fixtimes(int x, int y)
{
    int halfscale;
    halfscale = 1000;
    
    return (x/halfscale) * (y/halfscale);
}

public int fixdiv(int x, int y)
{
    int halfscale;
    halfscale = 1000;
    
    int d;
    d = y/halfscale/halfscale;
    if (d == 0)
        return 1;
    return x / d;
}

public int fixsqrt(int x)
{
    int y;
    int last_y;
    int epsilon;
    y = x/2;
    last_y = x;
    epsilon = 10000;
    
    while (fixabs(y - last_y) > epsilon)
    {
        if (y == 0)
            return y;
        last_y = y;
        if (2*y != 0)
            y = y - fixdiv(fixtimes(y, y) - x, 2*y);
    }
    
    return y;
}

int complexplus(int a, int b, int c, int d)
{
    return (a + c, b + d);
}

int complextimes(int a, int b, int c, int d)
{
    return (fixtimes(a, c) - fixtimes(b, d), fixtimes(a, d) + fixtimes(b, c));
}

void debug(int x);

int mandelbrot_test(int cr, int ci)
{
    int i;
    int max;
    int zr;
    int zi;
    int max_radius;
    max = 255;
    max_radius = 2000000;

    (zr, zi) = (0, 0);
    i = 0;
    while (i < max && fixtimes(zr, zr) + fixtimes(zi, zi) < max_radius*max_radius)
    {
        (zr, zi) = complextimes(zr, zi, zr, zi);
        (zr, zi) = complexplus(zr, zi, cr, ci);
        i = i + 1;
        //debug(i);
    }
    
    if (i == max)
        return 0;
    return i;
}

void output(int colour);

public void fractal()
{
    int i;
    i = -100000;
    while (i < 100000)
    {
        int j;
        j = -100000;
        while (j < 100000)
        {
            int colour;
            int x;
            int y;
            x = j - 383127;
            y = i + 650970;
            colour = mandelbrot_test(x, y);
            output(colour);
            j = j + 100;
        }
        i = i + 100;
        debug(i);
    }
}
