public int mf(int cr, int ci)
{
    int i = 0;
    int zr = 0;
    int zi = 0;
    
    while (i < 100 && zr * zr + zi * zi < 4)
    {
        int t = zr * zr - zi * zi + cr;
        zi = 2 * zr * zi + ci;
        zr = t;
        
        i = i + 1;
    }
    
    return i;
}
