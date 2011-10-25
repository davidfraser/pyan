#include "fractal.h"

#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#ifdef WIN32
    extern int omp_get_num_procs(void);
#else
    #include <omp.h>
#endif

static int frame;
static int num_frames;
static int frame_offset;
static int num_jobs;
static int width, height;


typedef struct BATON
{
    MFUNC *mfunc;
    int num_pixels;
    int pixels_per_job;
    int j;
    int *x_slots;
    int *y_slots;
    int *i_slots;
} BATON;


static MFUNC *baton_mfunc;


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
    baton_mfunc = mfunc;
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


void parallel_allocate_slots(int num_slots, BATON *baton)
{
    int i;
    
    baton->x_slots = malloc(sizeof(int) * num_slots);
    baton->y_slots = malloc(sizeof(int) * num_slots);
    baton->i_slots = malloc(sizeof(int) * num_slots);
    
    for (i = 0; i < num_slots; i++)
        baton->i_slots[i] = 0;    
}


int parallel_next_pixel(int slot, double *cx, double *cy, BATON *baton)
{
    int a;

    if (baton->i_slots[slot] >= baton->pixels_per_job)
        return 0;

    a = (baton->i_slots[slot] * num_jobs + baton->j) * num_frames + ((frame + frame_offset) % num_frames);
    if (a >= baton->num_pixels)
        return 0;
    
    baton->x_slots[slot] = a % width;
    baton->y_slots[slot] = a / width;

    *cx = (baton->x_slots[slot] - width/2.0)*scale + centrex;
    *cy = (baton->y_slots[slot] - height/2.0)*scale + centrey;
    
    baton->i_slots[slot]++;

    return 1;
}


void parallel_output_pixel(int slot, int k, double fx, double fy, BATON *baton)
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
    
    set_pixel(baton->x_slots[slot], baton->y_slots[slot], val);
}


void parallel_update(void)
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
        BATON baton;
        baton.mfunc = baton_mfunc;
        baton.num_pixels = width*height;
        baton.pixels_per_job = pixels_per_job;
        baton.j = j;
        baton_mfunc(max_iterations, parallel_allocate_slots, parallel_next_pixel, parallel_output_pixel, &baton);
    }
    pixels_done = old_pixels_done;
    for (j = 0; j < num_jobs; j++)
        pixels_done += thread_done[j];

    frame++;
}
