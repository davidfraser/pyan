#define _USE_MATH_DEFINES
#include <math.h>

extern int omp_get_num_threads();

static int frame;
static int num_frames;
static int frame_offset;
static int num_jobs;
static int width, height;


void parallel_init(int w, int h)
{
	frame = 0;
	num_frames = 43;
	frame_offset = 0;
	num_jobs = omp_get_num_threads();
	width = w;
	height = h;
}


void parallel_restart(void)
{
	frame_offset = frame;
	frame = 0;
}


extern int do_pixel(int x, int y);


void parallel_update(void)
{
	int num_pixels = width*height;
	int pixels_per_job = (int) ceil((double) num_pixels / num_frames / num_jobs);
	int j;

	if (frame >= num_frames)
		return;

	#pragma omp parallel for
	for (j = 0; j < num_jobs; j++)
	{
		int i;
		for (i = 0; i < pixels_per_job; i++)
		{
		    int a = (i * num_jobs + j) * num_frames + ((frame + frame_offset) % num_frames);
			if (a < num_pixels)
			{
		        int x = a % width;
				int y = a / width;

				do_pixel(x, y);
			}
		}
	}

	frame++;
}
