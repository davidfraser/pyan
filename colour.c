#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static int compare_values(const void *va, const void *vb)
{
    const float *a = va, *b = vb;
    if (*a < *b)
        return -1;
    else if (*a > *b)
        return 1;
    else
        return 0;
}


void build_colour_map(float *values, int num_values, float *map, int map_size)
{
    int i;
    float *sorted_values = malloc(sizeof(float) * num_values);
    if (!sorted_values)
        return;   //TODO handle out-of-memory more usefully!
    
    memmove(sorted_values, values, sizeof(float) * num_values);
    
    qsort(sorted_values, num_values, sizeof(float), compare_values);
    
    for (i = 0; i < map_size; i++)
    {
        int pos = i*(num_values/map_size);
        map[i] = sorted_values[pos];
    }
    
    free(sorted_values);
}


unsigned int map_colour(float x, float *map, unsigned int map_size)
{
    unsigned int p = 0, q = map_size - 1;
    
    /* What a pity that bsearch doesn't do range searches!  It's bound to be
       better code than my off-the-cuff binary search... */
    while (p < q)
    {
        unsigned int mp = (p + q)/2;
        
        /* mp and mp+1 are valid indices here, because p < q and mp is
           rounded down, hence the greatest mp can be is q-1. */
        if (map[mp] > x)
        {
            q = mp;
        }
        else if (map[mp+1] <= x)
        {
            p = mp+1;
        }
        else
        {
            return mp;
        }
    }
    
    return p;  /* Can reach here p is the last index in the map. */
}


#ifdef RUN_TEST
/**
 * Alternative version of map_colour with a linear algorithm; used for testing. */
 */
static unsigned int map_colour2(float x, float *map, unsigned int map_size)
{
    unsigned int p = 0;
    
    while (p < map_size-1 && map[p+1] <= x)
        p++;
    
    return p;
}


static void test(void)
{
    unsigned int i;
    unsigned int num_values = 10000000;
    unsigned int map_size = 256;
    
    float *buffer = malloc(sizeof(float) * num_values);
    float map[map_size];
    
    printf("Generating values\n");
    
    for (i = 0; i < num_values; i++)
    {
        buffer[i] = rand()/1000000000.0;
        if (i % 1000000 == 0)
        {
            printf("%f ", buffer[i]);
            fflush(stdout);
        }
    }
    
    printf("\nBulding map\n");
    
    build_colour_map(buffer, num_values, map, map_size);
    
    printf("\nMapping colours (10 times)\n");
    
    int j;
    for (j = 0; j < 10; j++)
    {
        for (i = 0; i < num_values; i++)
        {
            unsigned int c = map_colour(buffer[i], map, map_size);
            if (i % 1000000 == 0)
            {
                printf("%d ", c);
                fflush(stdout);
            }
        }
        printf("\n");
    }
    
    printf("Exiting\n");
    
    free(buffer);
}

int main(int argc, char *argv[])
{
    test();
    return 0;
}
#endif
