static int width, height;
static int i, j;


void simple_init(int w, int h)
{
	width = w;
	height = h;
    i = 0;
    j = 0;
}


void simple_restart(void)
{
    i = 0;
    j = 0;
}


extern int do_pixel(int x, int y);


void simple_update(void)
{
    int quota = 1000;
    
    while (quota > 0)
    {
        if (i >= height)
            return;
        
        if (j >= width)
        {
            j = 0;
            i++;
        }
        
        do_pixel(j, i);
        j++;
        quota--;
	}
}
