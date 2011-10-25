#include "mfunc.h"

#include <xmmintrin.h>

#define ENABLE_SLOT0 1
#define ENABLE_SLOT1 1
#define ENABLE_SLOT2 1
#define ENABLE_SLOT3 1

#if (!ENABLE_SLOT0) && (!ENABLE_SLOT1) && (!ENABLE_SLOT2) && (!ENABLE_SLOT3)
#error At least one slot must by enabled!
#endif

typedef union {
    __m128 m128;
    unsigned long int ints[4];
} int_union;

typedef union {
    __m128 m128;
    float floats[4];
} float_union;


static int check_slot(int slot, int *i, int_union *test, int *in_progress,
        float_union *cx, float_union *cy, float_union *zr, float_union *zi,
        int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
{
    if (*i < max_iterations && !test->ints[slot])
        return 1;
    
    float_union pixel_x, pixel_y;
    
    double px, py;

    if (*in_progress & (1 << slot))
    {
        pixel_x.m128 = zr->m128;
        pixel_y.m128 = zi->m128;
        output_pixel(slot, test->ints[slot] ? *i : 0, pixel_x.floats[slot], pixel_y.floats[slot], baton);
    }
    else
    {
        *in_progress |= (1 << slot);
    }

    if (next_pixel(slot, &px, &py, baton))
    {
        pixel_x.floats[slot] = px;
        pixel_y.floats[slot] = py;
        
        cx->floats[slot] = px;
        cy->floats[slot] = py;
        zr->floats[slot] = 0.0;
        zi->floats[slot] = 0.0;
    }
    else
    {
        *in_progress &= ~(1 << slot);
    }
    *i = 0;

    if (*in_progress == 0)
    {
        return 0;
    }
    
    return 1;
}


void mfunc_simd_float(int max_iterations, ALLOCATE_SLOTS allocate_slots, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel, BATON *baton)
{
    int i0 = max_iterations;
    int i1 = max_iterations;
    int i2 = max_iterations;
    int i3 = max_iterations;
    int in_progress = 0;

    __m128 cx;
    __m128 cy;
    __m128 zr;
    __m128 zi;
    __m128 zr2;
    __m128 zi2;
    __m128 t;
    __m128 boundary;
    __m128 zero;

    int_union test;
    
    allocate_slots(ENABLE_SLOT3 ? 4 : (ENABLE_SLOT2 ? 3 : (ENABLE_SLOT1 ? 2 : 1)), baton);
    
    boundary = _mm_set1_ps(2.0*2.0);
    zero = _mm_set1_ps(0.0);
    cx = _mm_set1_ps(0.0);
    cy = _mm_set1_ps(0.0);
    zr = _mm_set1_ps(0.0);
    zi = _mm_set1_ps(0.0);

    while (1)
    {
        /* Check if it's time to output the first pixel and/or start a new one. */
        if (ENABLE_SLOT0 && !check_slot(0, &i0, &test, &in_progress, (float_union*) &cx, (float_union*) &cy, (float_union*) &zr, (float_union*) &zi, max_iterations, next_pixel, output_pixel, baton))
            break;

        /* Check if it's time to output the second pixel and/or start a new one. */
        if (ENABLE_SLOT1 && !check_slot(1, &i1, &test, &in_progress, (float_union*) &cx, (float_union*) &cy, (float_union*) &zr, (float_union*) &zi, max_iterations, next_pixel, output_pixel, baton))
            break;

        /* Check if it's time to output the third pixel and/or start a new one. */
        if (ENABLE_SLOT2 && !check_slot(2, &i2, &test, &in_progress, (float_union*) &cx, (float_union*) &cy, (float_union*) &zr, (float_union*) &zi, max_iterations, next_pixel, output_pixel, baton))
            break;

        /* Check if it's time to output the fourth pixel and/or start a new one. */
        if (ENABLE_SLOT3 && !check_slot(3, &i3, &test, &in_progress, (float_union*) &cx, (float_union*) &cy, (float_union*) &zr, (float_union*) &zi, max_iterations, next_pixel, output_pixel, baton))
            break;

        /* Do some work on the current pixel. */
        zr2 = _mm_mul_ps(zr, zr);
        zi2 = _mm_mul_ps(zi, zi);
        t = _mm_mul_ps(zr, zi);
        zr = _mm_sub_ps(zr2, zi2);
        zr = _mm_add_ps(zr, cx);
        zi = _mm_add_ps(t, t);
        zi = _mm_add_ps(zi, cy);

        /* Check against the boundary. */
        t = _mm_add_ps(zr2, zi2);
        test.m128 = _mm_cmpgt_ps(t, boundary);

        if (ENABLE_SLOT0)
            i0++;
        if (ENABLE_SLOT1)
            i1++;
        if (ENABLE_SLOT2)
            i2++;
        if (ENABLE_SLOT3)
            i3++;
    }
}
