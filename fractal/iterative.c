#include "fractal.h"

#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


#define PIXEL_COST 50
#define QUOTA_SIZE 500000

#define ITERATION_DEPTH_START 4
#define ITERATION_DEPTH_FACTOR M_SQRT2


typedef struct BATON
{
    MFUNC *mfunc;
    int *x_slots;
    int *y_slots;
    int *done;
    double *point_x;
    double *point_y;
} BATON;


static int width, height;
static int i, j;
static int quota;
static BATON baton;
static int iteration_depth;


void iterative_init(int w, int h)
{
    width = w;
    height = h;
    i = 0;
    j = 0;
    baton.done = malloc(w * h * sizeof(int));
    baton.point_x = malloc(w * h * sizeof(double));
    baton.point_y = malloc(w * h * sizeof(double));
}


void iterative_restart(MFUNC mfunc)
{
    for (i = 0; i < width*height; i++)
    {
        baton.point_x[i] = 0.0;
        baton.point_y[i] = 0.0;
        baton.done[i] = 0;
    }
    
    i = 0;
    j = 0;
    baton.mfunc = mfunc;    
    iteration_depth = ITERATION_DEPTH_START;
}


void iterative_allocate_slots(int num_slots, BATON *baton)
{
    baton->x_slots = malloc(sizeof(int) * num_slots);
    baton->y_slots = malloc(sizeof(int) * num_slots);
}


int iterative_next_pixel(int slot, double *zx, double *zy, double *cx, double *cy, BATON *baton)
{
restart:
    if (i >= height)
    {
        if (iteration_depth >= max_iterations)
            return 0;
        
        i = 0;
        j = 0;
        iteration_depth *= ITERATION_DEPTH_FACTOR;
        pixels_done = 0;
    }
    
    if (quota <= 0 || i >= height)
        return 0;
    
    *zx = 0.0;
    *zy = 0.0;
    *cx = (j - width/2.0)*scale + centrex;
    *cy = (i - height/2.0)*scale + centrey;

    baton->x_slots[slot] = j;
    baton->y_slots[slot] = i;
    
    j++;

    if (j >= width)
    {
        j = 0;
        i++;
    }

    if (baton->done[i*width + j])
        goto restart;
    
    return 1;
}


void iterative_output_pixel(int slot, int k, double fx, double fy, BATON *baton)
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
    
    if (k == 0)
    {
        baton->point_x[baton->y_slots[slot] * width + baton->x_slots[slot]] = fx;
        baton->point_y[baton->y_slots[slot] * width + baton->x_slots[slot]] = fy;
    }
    else
    {
        baton->done[i*width + j] = 1;
    }    
    
    quota -= val;
    
    set_pixel(baton->x_slots[slot], baton->y_slots[slot], val);
    quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;
}


void iterative_update()
{
    quota = QUOTA_SIZE;

    baton.mfunc(iteration_depth, iterative_allocate_slots, iterative_next_pixel, iterative_output_pixel, &baton);
    
    status = "ITERATING";
}
