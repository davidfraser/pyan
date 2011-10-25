#include "fractal.h"

#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

extern int omp_get_num_procs();

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
    num_jobs = omp_get_num_procs();
    width = w;
    height = h;
}


void parallel_restart(MFUNC mfunc)
{
    frame_offset = frame;
    frame = 0;
}


void parallel_update_direct(void)
{
    int num_pixels = width*height;
    int pixels_per_job = (int) ceil((double) num_pixels / num_frames / num_jobs);
    int j;
    int old_pixels_done = pixels_done;
    int thread_done[16];
    
    memset(thread_done, 0, sizeof(thread_done));

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
                thread_done[j]++;
            }
        }
    }
    pixels_done = old_pixels_done;
    for (j = 0; j < num_jobs; j++)
        pixels_done += thread_done[j];

    frame++;
}


void parallel_update(void)
{
    parallel_update_direct();
}
