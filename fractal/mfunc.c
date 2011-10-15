#include "fractal.h"

#define _USE_MATH_DEFINES
#include <math.h>


int mfunc(double cx, double cy, int max_iterations, double *fx, double *fy)
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


void mfunc_loop(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel)
{
    int i = max_iterations;
    double cx, cy;
    double zr, zi;
    double zr2, zi2;
    int done = 0;
    
    while (1)
    {
        double t;
        
        /* Check if it's time to output a pixel and/or start a new one. */
        if (i >= max_iterations || zr2 + zi2 > 2.0*2.0)
        {
            if (done != 0)
            {
                if (zr2 + zi2 <= 2.0*2.0)
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
        zr2 = zr*zr;
        zi2 = zi*zi;
        t = zr*zi;
        zr = zr2 - zi2 + cx;
        zi = t + t + cy;

        i++;
    }
}


#include <pmmintrin.h>

#define ENABLE_SLOT0 1
#define ENABLE_SLOT1 1

void mfunc_simd(int max_iterations, PIXEL_SOURCE next_pixel,
PIXEL_OUTPUT output_pixel)
{
    int i0 = max_iterations;
    int i1 = max_iterations;
    int in_progress = 0;

    __m128d cx;
    __m128d cy;
    __m128d zr;
    __m128d zi;
    __m128d zr2;
    __m128d zi2;
    __m128d t;
    __m128d boundary;
    __m128d zero;

    union {
        __m128d m128d;
        unsigned long long int ints[2];
    } test;
    
    boundary = _mm_set1_pd(2.0*2.0);
    zero = _mm_set1_pd(0.0);
    cx = _mm_set1_pd(0.0);
    cy = _mm_set1_pd(0.0);
    zr = _mm_set1_pd(0.0);
    zi = _mm_set1_pd(0.0);

    while (1)
    {
        /* Check if it's time to output the first pixel and/or start a
new one. */
        if (ENABLE_SLOT0 && (i0 >= max_iterations || test.ints[0]))
        {
            union {
                __m128d m128d;
                double doubles[2];
            } pixel_x;

            union {
                __m128d m128d;
                double doubles[2];
            } pixel_y;

            if (in_progress & 1)
            {
                pixel_x.m128d = zr;
                pixel_y.m128d = zi;
                output_pixel(0, test.ints[0] ? i0 : 0, pixel_x.doubles[0], pixel_y.doubles[0]);
            }
            else
            {
                in_progress |= 1;
            }

            if (next_pixel(0, &pixel_x.doubles[0], &pixel_y.doubles[0]))
            {
                cx = _mm_move_sd(cx, pixel_x.m128d);
                cy = _mm_move_sd(cy, pixel_y.m128d);
                zr = _mm_move_sd(zr, zero);
                zi = _mm_move_sd(zi, zero);
            }
            else
            {
                in_progress &= ~1;
            }
            i0 = 0;

            if (in_progress == 0)
                break;
        }

        /* Check if it's time to output the second pixel and/or start
a new one. */
        if (ENABLE_SLOT1 && (i1 >= max_iterations || test.ints[1]))
        {
            union {
                __m128d m128d;
                double doubles[2];
            } pixel_x;

            union {
                __m128d m128d;
                double doubles[2];
            } pixel_y;

            if (in_progress & 2)
            {
                pixel_x.m128d = zr;
                pixel_y.m128d = zi;
                output_pixel(1, test.ints[1] ? i1 : 0, pixel_x.doubles[1], pixel_y.doubles[1]);
            }
            else
            {
                in_progress |= 2;
            }

            if (next_pixel(1, &pixel_x.doubles[1], &pixel_y.doubles[1]))
            {
                cx = _mm_move_sd(pixel_x.m128d, cx);
                cy = _mm_move_sd(pixel_y.m128d, cy);
                zr = _mm_move_sd(zero, zr);
                zi = _mm_move_sd(zero, zi);
            }
            else
            {
                in_progress &= ~2;
            }
            i1 = 0;

            if (in_progress == 0)
                break;
        }

        /* Do some work on the current pixel. */
        zr2 = _mm_mul_pd(zr, zr);
        zi2 = _mm_mul_pd(zi, zi);
        t = _mm_mul_pd(zr, zi);
        zr = _mm_sub_pd(zr2, zi2);
        zr = _mm_add_pd(zr, cx);
        zi = _mm_add_pd(t, t);
        zi = _mm_add_pd(zi, cy);

        /* Check against the boundary. */
        t = _mm_add_pd(zr2, zi2);
        test.m128d = _mm_cmpgt_pd(t, boundary);

        if (ENABLE_SLOT0)
            i0++;
        if (ENABLE_SLOT1)
            i1++;
    }
}
