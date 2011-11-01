#ifndef FRACTAL_H
#define FRACTAL_H

#include "mfunc.h"

extern float do_pixel(int x, int y);
extern void set_pixel(int x, int y, float k);

extern double centrex, centrey;
extern double scale;
extern int max_iterations;
extern int pixels_done;

extern char *status;

extern void simple_init(int w, int h);
extern void simple_restart(MFUNC mfunc);
extern void simple_update(void);

extern void parallel_init(int w, int h);
extern void parallel_restart(MFUNC mfunc);
extern void parallel_update(void);

extern void trace_init(int w, int h);
extern void trace_restart(MFUNC mfunc);
extern void trace_update(void);

extern void iterative_init(int w, int h);
extern void iterative_restart(MFUNC mfunc);
extern void iterative_update(void);

/** Build a colour map based on a distribution of values.  The map is an
 * ordered sequence of values; a value x will be mapped to i where map[i] is
 * the greatest element <= x.  This function builds the map using a uniform
 * distribution of the input, i.e. a similar number of items will be mapped
 * to each index.
 *
 * @param values array of values
 * @param num_values number of elements in @a values
 * @param map destination for map (allocated by caller)
 * @param map_size size of the @a map -- this is how many entries will be in it.
 */
extern void build_colour_map(float *values, int num_values, float *map, int map_size);


/** Map a value to an index in the colour map.
 *
 * @param x value to map
 * @param map the colour map (e.g. built by @a build_colour map)
 * @param map_size the size of the map
 * @return the index of this value in the map, an integer between 0 and @a map_size - 1.
 */
unsigned int map_colour(float x, float *map, unsigned int map_size);


#endif
