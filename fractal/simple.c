#include "fractal.h"

#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


#define PIXEL_COST 50
#define QUOTA_SIZE 500000


typedef struct BATON
{
    MFUNC *mfunc;
    int *x_slots;
    int *y_slots;
} BATON;


static int width, height;
static int i, j;
static int quota;
static BATON baton;


void simple_init(int w, int h)
{
    width = w;
    height = h;
    i = 0;
    j = 0;
}


void simple_restart(MFUNC mfunc)
{
    i = 0;
    j = 0;
    baton.mfunc = mfunc;
}


void simple_update_direct(void)
{
    quota = QUOTA_SIZE;

    while (quota > 0)
    {
        float val;
        
        if (j >= width)
        {
            j = 0;
            i++;
        }
        
        if (i >= height)
            return;
        
        val = do_pixel(j, i);
        quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;
        j++;
    }
}


void simple_allocate_slots(int num_slots, BATON *baton)
{
    baton->x_slots = malloc(sizeof(int) * num_slots);
    baton->y_slots = malloc(sizeof(int) * num_slots);
}


int simple_next_pixel(int slot, double *cx, double *cy, BATON *baton)
{
    if (quota <= 0 || i >= height)
        return 0;
    
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

    return 1;
}


void simple_output_pixel(int slot, int k, double fx, double fy, BATON *baton)
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
    
    quota -= val;
    
    set_pixel(baton->x_slots[slot], baton->y_slots[slot], val);
    quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;
}


void simple_update()
{
    quota = QUOTA_SIZE;

    baton.mfunc(max_iterations, simple_allocate_slots, simple_next_pixel, simple_output_pixel, &baton);
    
    status = "RENDERING";
}
