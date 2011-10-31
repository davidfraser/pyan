#include "mfunc.h"

#include <pmmintrin.h>

#define ENABLE_SLOT0 1
#define ENABLE_SLOT1 1

#if (!ENABLE_SLOT0) && (!ENABLE_SLOT1)
#error At least one slot must by enabled!
#endif

typedef union {
    __m128d m128d;
    unsigned long long int ints[2];
} int_union;    


static int check_slot(int slot, int *i, int_union *test, int *in_progress,
        __m128d *cx, __m128d *cy, __m128d *zr, __m128d *zi, __m128d *zero,
        int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
{
    union {
        __m128d m128d;
        double doubles[2];
    } pixel_x;

    union {
        __m128d m128d;
        double doubles[2];
    } pixel_y;

    if (*i < max_iterations && !test->ints[slot])
        return 1;
    
    if (*in_progress & (1 << slot))
    {
        pixel_x.m128d = *zr;
        pixel_y.m128d = *zi;
        output_pixel(slot, test->ints[slot] ? *i : 0, pixel_x.doubles[slot], pixel_y.doubles[slot], baton);
    }
    else
    {
        *in_progress |= (1 << slot);
    }

    if (next_pixel(slot, &pixel_x.doubles[slot], &pixel_y.doubles[slot], baton))
    {
        if (slot == 0)
        {
            *cx = _mm_move_sd(*cx, pixel_x.m128d);
            *cy = _mm_move_sd(*cy, pixel_y.m128d);
            *zr = _mm_move_sd(*zr, *zero);
            *zi = _mm_move_sd(*zi, *zero);
        }
        else
        {
            *cx = _mm_move_sd(pixel_x.m128d, *cx);
            *cy = _mm_move_sd(pixel_y.m128d, *cy);
            *zr = _mm_move_sd(*zero, *zr);
            *zi = _mm_move_sd(*zero, *zi);
        }
    }
    else
    {
        *in_progress &= ~(1 << slot);
    }
    *i = 0;

    if (*in_progress == 0)
        return 0;
    
    return 1;
}


void mfunc_simd(int max_iterations, ALLOCATE_SLOTS allocate_slots, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
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
    __m128d t, t2;
    __m128d boundary;
    __m128d zero;

    int_union test;
    
    int countdown_from;
    int countdown;

    allocate_slots(ENABLE_SLOT1 ? 2 : 1, baton);
    
    boundary = _mm_set1_pd(2.0*2.0);
    zero = _mm_set1_pd(0.0);
    cx = _mm_set1_pd(0.0);
    cy = _mm_set1_pd(0.0);
    zr = _mm_set1_pd(0.0);
    zi = _mm_set1_pd(0.0);

    while (1)
    {
        /* Check if it's time to output the first pixel and/or start a new one. */
        if (ENABLE_SLOT0 && !check_slot(0, &i0, &test, &in_progress, &cx, &cy, &zr, &zi, &zero, max_iterations, next_pixel, output_pixel, baton))
            break;
                
        /* Check if it's time to output the second pixel and/or start a new one. */
        if (ENABLE_SLOT1 && !check_slot(1, &i1, &test, &in_progress, &cx, &cy, &zr, &zi, &zero, max_iterations, next_pixel, output_pixel, baton))
            break;

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

        countdown_from = max_iterations - MAX(i0, i1);
        if (countdown_from <= 0)
            countdown_from = 1;
        countdown = countdown_from;

        while (1)
        {
            /* Do some work on the current pixel. */
            zr2 = _mm_mul_pd(zr, zr);
            zi2 = _mm_mul_pd(zi, zi);
            t = _mm_mul_pd(zr, zi);
            zr = _mm_sub_pd(zr2, zi2);
            zr = _mm_add_pd(zr, cx);
            zi = _mm_add_pd(t, t);
            zi = _mm_add_pd(zi, cy);

            countdown--;

            /* Check against the boundary. */
            t2 = _mm_add_pd(zr2, zi2);
            t2 = _mm_cmpgt_pd(t2, boundary);

            if (countdown == 0 || _mm_movemask_pd(t2))
                break;
        }
        test.m128d = t2;
        i0 += (countdown_from - countdown);
        i1 += (countdown_from - countdown);
    }
}
