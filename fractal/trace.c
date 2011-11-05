#include "fractal.h"

#include <string.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "pq.h"


#ifdef WIN32
    # pragma pack (1)
    typedef struct COORDS
    {
        unsigned long int x:12;
        unsigned long int y:12;
        signed long priority:8;
    } COORDS;
    # pragma pack ()
#else
    typedef struct COORDS
    {
        unsigned int x:12;
        unsigned int y:12;
        char priority:8;
    } COORDS;
#endif


#define NUM_SEEDS 1000
#define PIXEL_COST 50
#define QUOTA_SIZE 500000

#define HIGHEST_PRIORITY (-128)
#define LOWEST_PRIORITY 127

typedef enum { SEEDING, TRACING, EDGING, FILLING, WAITING } STATE;

typedef struct DRAWING
{
    WINDOW *window;
    FRACTAL *fractal;
    MFUNC *mfunc;
    GET_POINT *get_point;
    int num_slots;
    int *x_slots;
    int *y_slots;
    int quota;
    int width, height;
    PQ *pq;
    int *done;
    STATE state;
} DRAWING;


static const int dx[] = { -1, -1, -1, 0, 1, 1, 1, 0 };
static const int dy[] = { -1, 0, 1, 1, 1, 0, -1, -1 };


DRAWING *trace_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc)
{
    int i;
    DRAWING *drawing = malloc(sizeof(DRAWING));
    
    drawing->window = window;
    drawing->fractal = fractal;
    drawing->mfunc = mfunc;
    drawing->get_point = get_point;
    drawing->width = window->width;
    drawing->height = window->height;
    drawing->x_slots = NULL;
    drawing->y_slots = NULL;
    drawing->pq = pq_create(0, drawing->width*drawing->height*5);
    if (!drawing->pq)
    {
        fprintf(stderr, "Can't allocate PQ for %d items!", drawing->width*drawing->height*5);
        exit(1);
    }
    
    drawing->done = malloc(sizeof(int) * drawing->width * drawing->height);
    memset(drawing->done, 0, sizeof(int) * drawing->width * drawing->height);
    
    for (i = 0; i < NUM_SEEDS; i++)
    {
        COORDS c;
        c.x = rand() % drawing->width;
        c.y = rand() % drawing->height;
        c.priority = HIGHEST_PRIORITY;
        pq_push(drawing->pq, *(int *) &c, NULL);
    }

    drawing->state = SEEDING;
    
    return drawing;
}


static void push_edges(DRAWING *drawing)
{
    int i;

    for (i = 0; i < drawing->width; i++)
    {
        COORDS c2;

        if (!drawing->done[i])
        {
            c2.x = i;
            c2.y = 0;
            c2.priority = HIGHEST_PRIORITY;
            pq_push(drawing->pq, *(int *) &c2, NULL);
        }

        if (!drawing->done[(drawing->height-1)*drawing->width + i])
        {
            c2.x = i;
            c2.y = drawing->height-1;
            c2.priority = HIGHEST_PRIORITY;
            pq_push(drawing->pq, *(int *) &c2, NULL);
        }
    }

    for (i = 0; i < drawing->height; i++)
    {
        COORDS c2;

        if (!drawing->done[i*drawing->width])
        {
            c2.x = 0;
            c2.y = i;
            c2.priority = HIGHEST_PRIORITY;
            pq_push(drawing->pq, *(int *) &c2, NULL);
        }

        if (!drawing->done[i*drawing->width + drawing->width - 1])
        {
            c2.x = drawing->width - 1;
            c2.y = i;
            c2.priority = HIGHEST_PRIORITY;
            pq_push(drawing->pq, *(int *) &c2, NULL);
        }
    }

    drawing->state = EDGING;
}


static void catch_remaining(DRAWING *drawing)
{
    int i, j;

    for (i = 0; i < drawing->height; i++)
    {
        for (j = 0; j < drawing->width; j++)
        {
            COORDS c2;

            if (!drawing->done[i*drawing->width + j])
            {
                c2.x = j;
                c2.y = i;
                c2.priority = HIGHEST_PRIORITY;
                pq_push(drawing->pq, *(int *) &c2, NULL);
            }
        }
    }

    drawing->state = TRACING;
}


static void trace_allocate_slots(int num_slots, BATON *baton)
{
    DRAWING *drawing = (DRAWING *) baton;
    int i;
    
    drawing->num_slots = num_slots;
    
    drawing->x_slots = malloc(sizeof(int) * num_slots);
    drawing->y_slots = malloc(sizeof(int) * num_slots);
    
    for (i = 0; i < num_slots; i++)
    {
        drawing->x_slots[i] = -1;
        drawing->y_slots[i] = -1;
    }
}


static int trace_next_pixel(int slot, double *zx, double *zy, double *cx, double *cy, BATON *baton)
{
    DRAWING *drawing = (DRAWING *) baton;
    COORDS c;
    int i;

    if (drawing->quota <= 0)
    {
        drawing->x_slots[slot] = -1;
        drawing->y_slots[slot] = -1;
        return 0;
    }

restart:
    if (drawing->pq->num_items <= 0)
    {
        if (pixels_done < drawing->width*drawing->height)
        {
            catch_remaining(drawing);
            drawing->x_slots[slot] = -1;
            drawing->y_slots[slot] = -1;
            return 0;
        }
        drawing->state = WAITING;
        drawing->x_slots[slot] = -1;
        drawing->y_slots[slot] = -1;
        return 0;
    }

    pq_pop(drawing->pq, (int *) &c, NULL);
    if (drawing->done[c.y*drawing->width + c.x])
        goto restart;
        
    if (c.priority == LOWEST_PRIORITY)
    {
        if (drawing->state == TRACING || drawing->state == SEEDING)
            push_edges(drawing);
        else if (drawing->state == EDGING)
            drawing->state = FILLING;
    }
    else if (drawing->state == SEEDING)
        drawing->state = TRACING;

    if (drawing->state == FILLING)
    {
        float val = 0;
        set_pixel(drawing->window, c.x, c.y, val);
        drawing->done[c.y*drawing->width + c.x] = 1;
        drawing->quota -= PIXEL_COST;

        for (i = 0; i < 8; i++)
        {
            COORDS c2;
            int new_x, new_y;
            new_x = c.x + dx[i];
            new_y = c.y + dy[i];
            if (new_x < 0 || new_y < 0 || new_x >= drawing->width || new_y >= drawing->height)
                continue;
            c2.x = new_x;
            c2.y = new_y;
            c2.priority = LOWEST_PRIORITY - ((new_x ^ new_y ^ drawing->quota) & 0x15);
            pq_push(drawing->pq, *(int *) &c2, NULL);
        }

        if (drawing->quota <= 0)
            return 0;
        goto restart;
    }
    
    for (i = 0; i < drawing->num_slots; i++)
    {
        if (i == slot)
            continue;
        
        if (c.x == drawing->x_slots[i] && c.y == drawing->y_slots[i])
            goto restart;
    }
        
    drawing->get_point(drawing->fractal, c.x, c.y, zx, zy, cx, cy);

    drawing->x_slots[slot] = c.x;
    drawing->y_slots[slot] = c.y;

    return 1;
}


static void trace_output_pixel(int slot, int k, double fx, double fy, BATON *baton)
{
    DRAWING *drawing = (DRAWING *) baton;    
    int i;
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
    
    set_pixel(drawing->window, drawing->x_slots[slot], drawing->y_slots[slot], val);
    drawing->done[drawing->y_slots[slot]*drawing->width + drawing->x_slots[slot]] = 1;
    drawing->quota -= ((k == 0) ? drawing->window->depth : k) + PIXEL_COST;

    for (i = 0; i < 8; i++)
    {
        COORDS c2;
        int priority;
        int new_x, new_y;

        new_x = drawing->x_slots[slot] + dx[i];
        new_y = drawing->y_slots[slot] + dy[i];
        if (new_x < 0 || new_y < 0 || new_x >= drawing->width || new_y >= drawing->height)
            continue;
        c2.x = new_x;
        c2.y = new_y;
        priority = (k == 0) ? LOWEST_PRIORITY : (int) (HIGHEST_PRIORITY*log(val)/log(drawing->window->depth) + ((new_x ^ new_y ^ drawing->quota) & 0x15));
        if (priority < HIGHEST_PRIORITY)
            priority = HIGHEST_PRIORITY;
        else if (priority > LOWEST_PRIORITY)
            priority = LOWEST_PRIORITY;
        c2.priority = priority;
        pq_push(drawing->pq, *(int *) &c2, NULL);
    }
}


void trace_update(DRAWING *drawing)
{
    drawing->quota = QUOTA_SIZE;

    drawing->mfunc(drawing->window->depth, trace_allocate_slots, trace_next_pixel, trace_output_pixel, (BATON *) drawing);

    if (drawing->state == SEEDING)
        status = "SEEDING";
    else if (drawing->state == TRACING)
        status = "TRACING";
    else if (drawing->state == EDGING)
        status = "EDGING";
    else if (drawing->state == FILLING)
        status = "FILLING";
    else if (drawing->state == WAITING)
        status = "WAITING";
    else
        status = "UNKNOWN";
}


void trace_destroy(DRAWING *drawing)
{
    free(drawing->x_slots);
    free(drawing->y_slots);
    free(drawing->done);
    pq_destroy(drawing->pq);
    free(drawing);
}
