#include "fractal.h"

#define _USE_MATH_DEFINES
#include <math.h>


int mfunc(double cx, double cy, int max_iterations, double *fx, double *fy)
{
	int i = 0;
	double zr = 0.0, zi = 0.0;

	while (i < max_iterations && zr*zr + zi*zi < 2.0*2.0)
	{
		double t = zr;
		zr = zr*zr - zi*zi + cx;
		zi = 2*t*zi + cy;
		i++;
	}
	*fx = zr;
	*fy = zi;

	if (zr*zr + zi*zi < 2.0*2.0)
		return 0;

	return i;
}


void mfunc_loop(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel)
{
    int i = max_iterations;
    double cx, cy;
    double zr, zi;
    int done = 0;
    
    while (1)
    {
        double t;
        
        /* Check if it's time to output a pixel and/or start a new one. */
        if (i >= max_iterations || zr*zr + zi*zi > 2.0*2.0)
        {
            if (done != 0)
            {
                if (zr*zr + zi*zi < 2.0*2.0)
                    output_pixel(0, 0, zr, zi);
                else
                    output_pixel(0, i, zr, zi);
            }
            
            if (!next_pixel(0, &cx, &cy))
                break;
            
            done += 1;
            
            zr = 0.0;
            zi = 0.0;
            i = 0;
        }
    
        /* Do some work on the current pixel. */
		t = zr;
		zr = zr*zr - zi*zi + cx;
		zi = 2*t*zi + cy;
		i++;
    }
}
