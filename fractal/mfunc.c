#include "fractal.h"

#define _USE_MATH_DEFINES
#include <math.h>


int mfunc_direct(double cx, double cy, int max_iterations, double *fx, double *fy)
{
    int i = 0;
    double zr = 0.0, zi = 0.0;
    double zr2 = 0.0, zi2 = 0.0;

    while (i < max_iterations && zr2 + zi2 < 2.0*2.0)
    {
        double t;

        zr2 = zr*zr;
        zi2 = zi*zi;
        t = zr*zi;
        zr = zr2 - zi2 + cx;
        zi = t + t + cy;

        i++;
    }
    *fx = zr;
    *fy = zi;

    if (zr2 + zi2 < 2.0*2.0)
        return 0;

    return i;
}


int mfunc_direct_float(double cx, double cy, int max_iterations, double *fx, double *fy)
{
    int i = 0;
    float zr = 0.0, zi = 0.0;
    float zr2 = 0.0, zi2 = 0.0;

    while (i < max_iterations && zr2 + zi2 < 2.0*2.0)
    {
        float t;

        zr2 = zr*zr;
        zi2 = zi*zi;
        t = zr*zi;
        zr = zr2 - zi2 + (float) cx;
        zi = t + t + (float) cy;

        i++;
    }
    *fx = zr;
    *fy = zi;

    if (zr2 + zi2 < 2.0*2.0)
        return 0;

    return i;
}


#define FIX_SEMI_SCALE 8192
#define FIX_SCALE (FIX_SEMI_SCALE*FIX_SEMI_SCALE)
#define TO_FIX(x) ((x) * FIX_SCALE)
#define FIX_TIMES(x, y) ((x) / FIX_SEMI_SCALE) * ((y) / FIX_SEMI_SCALE)
#define FROM_FIX(x) ((x) / (double) FIX_SCALE)


int mfunc_direct_int(double cx, double cy, int max_iterations, double *fx, double *fy)
{
    int i = 0;
    long int zr = 0, zi = 0;
    long int zr2 = 0, zi2 = 0;

    long int boundary = TO_FIX(2.0*2.0);

    long int cx_fix = TO_FIX(cx);
    long int cy_fix = TO_FIX(cy);

    while (i < max_iterations && zr2 + zi2 < boundary)
    {
        double t;

        zr2 = FIX_TIMES(zr, zr);
        zi2 = FIX_TIMES(zi, zi);
        t = FIX_TIMES(zr, zi);
        zr = zr2 - zi2 + cx_fix;
        zi = t + t + cy_fix;

        i++;
    }
    *fx = FROM_FIX(zr);
    *fy = FROM_FIX(zi);

    if (zr2 + zi2 < boundary)
        return 0;

    return i;
}


void mfunc_loop(int max_iterations, ALLOCATE_SLOTS allocate_slots, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
{
    int i = max_iterations;
    double cx, cy;
    double zr, zi;
    double zr2, zi2;
    int done = 0;
    
    allocate_slots(1, baton);
    
    while (1)
    {
        double t;
        
        /* Check if it's time to output a pixel and/or start a new one. */
        if (i >= max_iterations || zr2 + zi2 > 2.0*2.0)
        {
            if (done != 0)
            {
                if (zr2 + zi2 <= 2.0*2.0)
                    output_pixel(0, 0, zr, zi, baton);
                else
                    output_pixel(0, i, zr, zi, baton);
            }
            
            if (!next_pixel(0, &cx, &cy, baton))
                break;
            
            done += 1;
            
            zr = 0.0;
            zi = 0.0;
            i = 0;
        }
    
        /* Do some work on the current pixel. */
        zr2 = zr*zr;
        zi2 = zi*zi;
        t = zr*zi;
        zr = zr2 - zi2 + cx;
        zi = t + t + cy;

        i++;
    }
}


void mfunc_loop_float(int max_iterations, ALLOCATE_SLOTS allocate_slots, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
{
    allocate_slots(1, baton);

    while (1)
    {
        double px, py;
        int k;
        double fx, fy;

        if (!next_pixel(0, &px, &py, baton))
            break;

        k = mfunc_direct_float(px, py, max_iterations, &fx, &fy);

        output_pixel(0, k, fx, fy, baton);
    }
}


void mfunc_loop_int(int max_iterations, ALLOCATE_SLOTS allocate_slots, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
{
    allocate_slots(1, baton);

    while (1)
    {
        double px, py;
        int k;
        double fx, fy;

        if (!next_pixel(0, &px, &py, baton))
            break;

        k = mfunc_direct_int(px, py, max_iterations, &fx, &fy);

        output_pixel(0, k, fx, fy, baton);
    }
}
