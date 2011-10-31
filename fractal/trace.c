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


typedef struct BATON
{
    MFUNC *mfunc;
    int num_slots;
    int *x_slots;
    int *y_slots;
} BATON;


static int width, height;
static PQ *pq = NULL;
static int *done;
static enum { SEEDING, TRACING, EDGING, FILLING, WAITING } state;
static BATON baton;


void trace_init(int w, int h)
{
    width = w;
    height = h;
    if (!pq)
        pq = pq_create(0, width*height*5);
    if (!pq)
    {
        fprintf(stderr, "Can't allocate PQ for %d items!", width*height*5);
        exit(1);
    }
    done = malloc(sizeof(int) * width * height);
}


#define NUM_SEEDS 1000
#define PIXEL_COST 50
#define QUOTA_SIZE 500000


void trace_restart(MFUNC mfunc)
{
    int i;
    pq->num_items = 0;
    for (i = 0; i < NUM_SEEDS; i++)
    {
        COORDS c;
        c.x = rand() % width;
        c.y = rand() % height;
        c.priority = -(i & 15);
        pq_push(pq, *(int *) &c, NULL);
    }

    memset(done, 0, sizeof(int)*width*height);
    state = SEEDING;
    
    baton.mfunc = mfunc;
}


static void push_edges(void)
{
    int i;

    for (i = 0; i < width; i++)
    {
        COORDS c2;

        if (!done[i])
        {
            c2.x = i;
            c2.y = 0;
            c2.priority = -10;
            pq_push(pq, *(int *) &c2, NULL);
        }

        if (!done[(height-1)*width + i])
        {
            c2.x = i;
            c2.y = height-1;
            c2.priority = -10;
            pq_push(pq, *(int *) &c2, NULL);
        }
    }

    for (i = 0; i < height; i++)
    {
        COORDS c2;

        if (!done[i*width])
        {
            c2.x = 0;
            c2.y = i;
            c2.priority = -10;
            pq_push(pq, *(int *) &c2, NULL);
        }

        if (!done[i*width + width - 1])
        {
            c2.x = width - 1;
            c2.y = i;
            c2.priority = -10;
            pq_push(pq, *(int *) &c2, NULL);
        }
    }

    state = EDGING;
}


static void catch_remaining(void)
{
    int i, j;

    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            COORDS c2;

            if (!done[i*width + j])
            {
                c2.x = j;
                c2.y = i;
                c2.priority = -10;
                pq_push(pq, *(int *) &c2, NULL);
            }
        }
    }

    state = TRACING;
}


void trace_update_direct(void)
{
    int quota = QUOTA_SIZE;

    while (quota > 0)
    {
        COORDS c;
        static int dx[] = { -1, -1, -1, 0, 1, 1, 1, 0 };
        static int dy[] = { -1, 0, 1, 1, 1, 0, -1, -1 };
        int i;
        int val;

        if (pq->num_items <= 0)
        {
            if (pixels_done < width*height)
            {
                catch_remaining();
                continue;
            }
            state = WAITING;
            break;
        }

        pq_pop(pq, (int *) &c, NULL);
        if (done[c.y*width + c.x])
            continue;
        
        if (c.priority == 0)
        {
            if (state == TRACING || state == SEEDING)
                push_edges();
            else if (state == EDGING)
                state = FILLING;
        }
        else if (state == SEEDING)
            state = TRACING;

        if (state == FILLING)
        {
            val = 0;
            set_pixel(c.x, c.y, val);
            quota -= PIXEL_COST;
        }
        else
        {
            val = do_pixel(c.x, c.y);

            quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;
        }
        done[c.y*width + c.x] = 1;

        for (i = 0; i < 8; i++)
        {
            COORDS c2;
            int priority;
            c2.x = c.x + dx[i];
            c2.y = c.y + dy[i];
            if (c2.x < 0 || c2.y < 0 || c2.x >= width || c2.y >= height)
                continue;
            priority = (val == 0) ? 0 : (-10-val-((c2.x ^ c2.y ^ quota) & 15));
            if (priority < -128)
                priority = -128;
            else if (priority > 127)
                priority = 127;
            c2.priority = priority;
            pq_push(pq, *(int *) &c2, NULL);
        }
    }

    if (state == SEEDING)
        status = "SEEDING";
    else if (state == TRACING)
        status = "TRACING";
    else if (state == EDGING)
        status = "EDGING";
    else if (state == FILLING)
        status = "FILLING";
    else if (state == WAITING)
        status = "WAITING";
    else
        status = "UNKNOWN";
}


static int quota;


static void trace_allocate_slots(int num_slots, BATON *baton)
{
    int i;
    
    baton->num_slots = num_slots;
    
    baton->x_slots = malloc(sizeof(int) * num_slots);
    baton->y_slots = malloc(sizeof(int) * num_slots);
    
    for (i = 0; i < num_slots; i++)
    {
        baton->x_slots[i] = -1;
        baton->y_slots[i] = -1;
    }
}


int trace_next_pixel(int slot, double *cx, double *cy, BATON *baton)
{
    COORDS c;
    int i;

    if (quota <= 0)
    {
        baton->x_slots[slot] = -1;
        baton->y_slots[slot] = -1;
        return 0;
    }

restart:
    if (pq->num_items <= 0)
    {
        if (pixels_done < width*height)
        {
            catch_remaining();
            baton->x_slots[slot] = -1;
            baton->y_slots[slot] = -1;
            return 0;
        }
        state = WAITING;
        baton->x_slots[slot] = -1;
        baton->y_slots[slot] = -1;
        return 0;
    }

    pq_pop(pq, (int *) &c, NULL);
    if (done[c.y*width + c.x])
        goto restart;
        
    if (c.priority == 0)
    {
        if (state == TRACING || state == SEEDING)
            push_edges();
        else if (state == EDGING)
            state = FILLING;
    }
    else if (state == SEEDING)
        state = TRACING;

    if (state == FILLING)
    {
        int val = 0;
        set_pixel(c.x, c.y, val);
        done[c.y*width + c.x] = 1;
        quota -= PIXEL_COST;
        goto restart;
    }
    
    for (i = 0; i < baton->num_slots; i++)
    {
        if (i == slot)
            continue;
        
        if (c.x == baton->x_slots[i] && c.y == baton->y_slots[i])
            goto restart;
    }

    *cx = (c.x - width/2.0)*scale + centrex;
    *cy = (c.y - height/2.0)*scale + centrey;

    baton->x_slots[slot] = c.x;
    baton->y_slots[slot] = c.y;

    return 1;
}


void trace_output_pixel(int slot, int k, double fx, double fy, BATON *baton)
{
    static int dx[] = { -1, -1, -1, 0, 1, 1, 1, 0 };
    static int dy[] = { -1, 0, 1, 1, 1, 0, -1, -1 };
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
    
    set_pixel(baton->x_slots[slot], baton->y_slots[slot], val);
    done[baton->y_slots[slot]*width + baton->x_slots[slot]] = 1;
    quota -= ((val == 0) ? max_iterations : val) + PIXEL_COST;

    for (i = 0; i < 8; i++)
    {
        COORDS c2;
        int priority;
        c2.x = baton->x_slots[slot] + dx[i];
        c2.y = baton->y_slots[slot] + dy[i];
        if (c2.x < 0 || c2.y < 0 || c2.x >= width || c2.y >= height)
            continue;
        priority = (val == 0) ? 0 : (-10-val-((c2.x ^ c2.y ^ quota) & 15));
        if (priority < -128)
            priority = -128;
        else if (priority > 127)
            priority = 127;
        c2.priority = priority;
        pq_push(pq, *(int *) &c2, NULL);
    }
}


void trace_update(void)
{
    quota = QUOTA_SIZE;

    baton.mfunc(max_iterations, trace_allocate_slots, trace_next_pixel, trace_output_pixel, &baton);

    if (state == SEEDING)
        status = "SEEDING";
    else if (state == TRACING)
        status = "TRACING";
    else if (state == EDGING)
        status = "EDGING";
    else if (state == FILLING)
        status = "FILLING";
    else if (state == WAITING)
        status = "WAITING";
    else
        status = "UNKNOWN";
}
