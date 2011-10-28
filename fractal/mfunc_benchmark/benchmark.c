#include <stdio.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <math.h>


typedef int PIXEL_SOURCE(int slot, double *cx, double *cy);
typedef void PIXEL_OUTPUT(int slot, int value, double fx, double fy);


void mfunc_loop(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel)
{
    int i = max_iterations;
    double cx = 0.0, cy = 0.0;
    double zr = 0.0, zi = 0.0;
	double zr2 = 0.0, zi2 = 0.0;
    int in_progress = 0;
    
    while (1)
    {
        double t;
        
        /* Check if it's time to output a pixel and/or start a new one. */
        if (i >= max_iterations || zr2 + zi2 > 2.0*2.0)
        {
            if (in_progress)
            {
                if (zr2 + zi2 < 2.0*2.0)
                    output_pixel(0, i, zr, zi);
                else
                    output_pixel(0, i, zr, zi);
            }
			else
			{
				in_progress = 1;
			}
            
            if (!next_pixel(0, &cx, &cy))
                break;
            
            zr = 0.0;
            zi = 0.0;
            i = 0;
        }
    
		/* Do some work on the current pixel. */
		zr2 = zr*zr;
		zi2 = zi*zi;
		t = zr * zi;
		zr = zr2 - zi2 + cx;
		zi = (t + t) + cy;

		i++;
    }
}


#include <pmmintrin.h>

#define ENABLE_SLOT0 1
#define ENABLE_SLOT1 1


void mfunc_simd0(int max_iterations, PIXEL_SOURCE next_pixel,
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
                output_pixel(0, test.ints[0] ? i0 : i0, pixel_x.doubles[0], pixel_y.doubles[0]);
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
                output_pixel(1, test.ints[1] ? i1 : i1, pixel_x.doubles[1], pixel_y.doubles[1]);
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
    __m128d t, t2;
    __m128d boundary;
    __m128d zero;

	int countdown_from;
	int countdown;

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

restart:
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
                output_pixel(0, test.ints[0] ? i0 : i0, pixel_x.doubles[0], pixel_y.doubles[0]);
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
                return;
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
                output_pixel(1, test.ints[1] ? i1 : i1, pixel_x.doubles[1], pixel_y.doubles[1]);
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
                return;
        }

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
			if (countdown == 0)
				break;

			t2 = _mm_add_pd(zr2, zi2);
			t2 = _mm_cmpgt_pd(t2, boundary);

			if (_mm_movemask_pd(t2))
				break;
		}
		if (countdown == 0)
		{
			t2 = _mm_add_pd(zr2, zi2);
			t2 = _mm_cmpgt_pd(t2, boundary);
		}
		test.m128d = t2;
		i0 += (countdown_from - countdown);
		i1 += (countdown_from - countdown);
    goto restart;
}


static int num;
static clock_t start_time, end_time;
static unsigned long long int valsum;
static double sums[2];

#define DEPTH 10000
#define SIZE 1000000

int test_next_pixel(int slot, double *cx, double *cy)
{
	if (num <= 0)
		return 0;
	
	*cx = (num%100)/250.0;
	*cy = ((num/100)%100)/250.0;

	num--;
	return 1;
}


void test_output_pixel(int slot, int value, double fx, double fy)
{
	valsum += value;
	sums[0] += fx;
	sums[1] += fy;
}

int main()
{
	float speed1, speed2;

	num = SIZE;
	valsum = 0;
	sums[0] = 0;
	sums[1] = 0;
    start_time = clock();
	mfunc_loop(DEPTH, test_next_pixel, test_output_pixel);
	end_time = clock();
	speed1 = SIZE / ((end_time - start_time) / (float) CLOCKS_PER_SEC);
	printf("LOOP speed was %f\n", speed1);
	printf("Sums are %lld, %f, %f\n", valsum, sums[0], sums[1]);

	num = SIZE;
	valsum = 0;
	sums[0] = 0;
	sums[1] = 0;
    start_time = clock();
	mfunc_simd0(DEPTH, test_next_pixel, test_output_pixel);
	end_time = clock();
	speed2 = SIZE / ((end_time - start_time) / (float) CLOCKS_PER_SEC);
	printf("SIMD0 speed was %f\n", speed2);
	printf("Sums are %lld, %f, %f\n", valsum, sums[0], sums[1]);
	printf("Speedup was %f percent\n", 100*(speed2 - speed1)/speed1);

	num = SIZE;
	valsum = 0;
	sums[0] = 0;
	sums[1] = 0;
    start_time = clock();
	mfunc_simd(DEPTH, test_next_pixel, test_output_pixel);
	end_time = clock();
	speed2 = SIZE / ((end_time - start_time) / (float) CLOCKS_PER_SEC);
	printf("SIMD speed was %f\n", speed2);
	printf("Sums are %lld, %f, %f\n", valsum, sums[0], sums[1]);
	printf("Speedup was %f percent\n", 100*(speed2 - speed1)/speed1);

	getchar();
}
