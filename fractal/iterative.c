#include "fractal.h"

#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


#define PIXEL_COST 50
#define QUOTA_SIZE 500000

#define ITERATION_DEPTH_START 4
#define ITERATION_DEPTH_FACTOR M_SQRT2


typedef struct DRAWING
{
    WINDOW *window;
    FRACTAL *fractal;
    MFUNC *mfunc;
    GET_POINT *get_point;
    int *x_slots;
    int *y_slots;
    int *done;
    double *point_x;
    double *point_y;
    int width, height;
    int i, j;
    int quota;
    int iteration_depth;
} DRAWING;


DRAWING *iterative_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc)
{
    int i;
    
    DRAWING *drawing = malloc(sizeof(DRAWING));
    drawing->window = window;
    drawing->fractal = fractal;
    drawing->mfunc = mfunc;
    drawing->get_point = get_point;
    drawing->width = window->width;
    drawing->height = window->height;
    drawing->i = 0;
    drawing->j = 0;
    drawing->x_slots = NULL;
    drawing->y_slots = NULL;
    drawing->done = malloc(drawing->width * drawing->height * sizeof(int));
    drawing->point_x = malloc(drawing->width * drawing->height * sizeof(double));
    drawing->point_y = malloc(drawing->width * drawing->height * sizeof(double));
    drawing->iteration_depth = ITERATION_DEPTH_START;
    
    for (i = 0; i < drawing->width * drawing->height; i++)
    {
        drawing->point_x[i] = 0.0;
        drawing->point_y[i] = 0.0;
        drawing->done[i] = 0;
    }
    
    return drawing;
}


static void iterative_allocate_slots(int num_slots, BATON *baton)
{
    DRAWING *drawing = (DRAWING *) baton;
    
    drawing->x_slots = malloc(sizeof(int) * num_slots);
    drawing->y_slots = malloc(sizeof(int) * num_slots);
}


static int iterative_next_pixel(int slot, double *zx, double *zy, double *cx, double *cy, BATON *baton)
{
    DRAWING *drawing = (DRAWING *) baton;
    
restart:
    if (drawing->i >= drawing->height)
    {
        if (drawing->iteration_depth >= drawing->window->depth)
            return 0;
        
        drawing->i = 0;
        drawing->j = 0;
        drawing->iteration_depth *= ITERATION_DEPTH_FACTOR;
        pixels_done = 0;
    }
    
    if (drawing->quota <= 0 || drawing->i >= drawing->height)
        return 0;
    
    drawing->get_point(drawing->fractal, drawing->j, drawing->i, zx, zy, cx, cy);

    drawing->x_slots[slot] = drawing->j;
    drawing->y_slots[slot] = drawing->i;
    
    drawing->j++;

    if (drawing->j >= drawing->width)
    {
        drawing->j = 0;
        drawing->i++;
    }

    if (drawing->done[drawing->y_slots[slot]*drawing->width + drawing->x_slots[slot]])
    {
        goto restart;
    }
    
    return 1;
}


static void iterative_output_pixel(int slot, int k, double fx, double fy, BATON *baton)
{
    DRAWING *drawing = (DRAWING *) baton;
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
        drawing->point_x[drawing->y_slots[slot] * drawing->width + drawing->x_slots[slot]] = fx;
        drawing->point_y[drawing->y_slots[slot] * drawing->width + drawing->x_slots[slot]] = fy;
    }
    else
    {
        drawing->done[drawing->y_slots[slot]*drawing->width + drawing->x_slots[slot]] = 1;
        set_pixel(drawing->window, drawing->x_slots[slot], drawing->y_slots[slot], val);
    }
    
    drawing->quota -= ((k == 0) ? drawing->iteration_depth : k) + PIXEL_COST;
}


void iterative_update(DRAWING *drawing)
{
    drawing->quota = QUOTA_SIZE;

    drawing->mfunc(drawing->iteration_depth, iterative_allocate_slots, iterative_next_pixel, iterative_output_pixel, (BATON *) drawing);
    
    status = "ITERATING";
}


void iterative_destroy(DRAWING *drawing)
{
    free(drawing->x_slots);
    free(drawing->y_slots);
    free(drawing->done);
    free(drawing->point_x);
    free(drawing->point_y);
    free(drawing);
}
