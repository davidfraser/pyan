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


typedef struct DRAWING
{
    WINDOW *window;
    FRACTAL *fractal;
    MFUNC *mfunc;
    GET_POINT *get_point;
    int num_slots;
    int width, height;
    
    int num_frames;
    int num_jobs;
    int num_pixels;
    int pixels_per_job;
    
    int frame;
    int frame_offset;
} DRAWING;


typedef struct BATON
{
    DRAWING *drawing;
    int i, j;
    int done;
    int *x_slots;
    int *y_slots;
} BATON;


DRAWING *parallel_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc)
{
    DRAWING *drawing = malloc(sizeof(DRAWING));
    
    drawing->window = window;
    drawing->fractal = fractal;
    drawing->mfunc = mfunc;
    drawing->get_point = get_point;
    drawing->width = window->width;
    drawing->height = window->height;
    
    drawing->num_jobs = omp_get_num_procs();
    drawing->num_pixels = drawing->width * drawing->height;
    drawing->num_frames = 43;
    drawing->pixels_per_job = (int) ceil((double) drawing->num_pixels / drawing->num_frames / drawing->num_jobs);
    
    drawing->frame_offset = 0;   //TODO use last frame?
    drawing->frame = 0;
    
    return drawing;
}


static void parallel_allocate_slots(int num_slots, BATON *baton)
{
    baton->x_slots = malloc(sizeof(int) * num_slots);
    baton->y_slots = malloc(sizeof(int) * num_slots);
}


int parallel_next_pixel(int slot, double *zx, double *zy, double *cx, double *cy, BATON *baton)
{
    int a;

    if (baton->i >= baton->drawing->pixels_per_job)
        return 0;

    a = (baton->i * baton->drawing->num_jobs + baton->j) * baton->drawing->num_frames + ((baton->drawing->frame + baton->drawing->frame_offset) % baton->drawing->num_frames);
    if (a >= baton->drawing->num_pixels)
        return 0;
    
    baton->x_slots[slot] = a % baton->drawing->width;
    baton->y_slots[slot] = a / baton->drawing->width;

    baton->drawing->get_point(baton->drawing->fractal, baton->x_slots[slot], baton->y_slots[slot], zx, zy, cx, cy);

    baton->i++;
    
    baton->done++;

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


void parallel_update(DRAWING *drawing)
{
    int j;
    int old_pixels_done = pixels_done;
    int thread_done[16];
    
    memset(thread_done, 0, sizeof(thread_done));

    if (drawing->frame >= drawing->num_frames)
        return;

    #pragma omp parallel for
    for (j = 0; j < drawing->num_jobs; j++)
    {
        BATON baton;
        baton.drawing = drawing;
        baton.j = j;
        baton.done = 0;
        baton.i = 0;    
        drawing->mfunc(max_iterations, parallel_allocate_slots, parallel_next_pixel, parallel_output_pixel, &baton);
        thread_done[j] = baton.done;
    }
    pixels_done = old_pixels_done;
    for (j = 0; j < drawing->num_jobs; j++)
        pixels_done += thread_done[j];

    drawing->frame++;
}


void parallel_destroy(DRAWING *drawing)
{
    free(drawing);
}
