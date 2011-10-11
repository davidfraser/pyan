#include "fractal.h"

#define _USE_MATH_DEFINES
#include <math.h>


#define PIXEL_COST 50
#define QUOTA_SIZE 500000


static int width, height;
static int i, j;
static int quota;


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


int simple_next_pixel(int slot, double *cx, double *cy)
{
    if (quota <= 0 || i >= height)
        return 0;
    
	*cx = (j - width/2.0)*scale + centrex;
	*cy = (i - height/2.0)*scale + centrey;
    
    return 1;
}


void simple_output_pixel(int slot, int k, double fx, double fy)
{
    float val = 0.0;
	if (k == 0)
	{
		val = 0.0;
	}
	else
	{
		float z = sqrt(fx*fx + fy*fy);
		val = (float) k - log(log(z))/log(2.0);
	}
    
    quota -= val;
    
    set_pixel(j, i, val);
    quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;
    j++;

    if (j >= width)
    {
        j = 0;
        i++;
    }
}


#define USE_LOOP 1


void simple_update(void)
{
    quota = QUOTA_SIZE;
#if USE_LOOP
    mfunc_loop(max_iterations, simple_next_pixel, simple_output_pixel);
#else

    while (quota > 0)
    {
        float val;
        
        if (j >= width)
        {
            j = 0;
            i++;
        }
        
        if (i >= height)
            return;
        
        val = do_pixel(j, i);
        quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;
        j++;
	}
#endif
}
