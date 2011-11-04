#ifndef FRACTAL_H
#define FRACTAL_H

#include "mfunc.h"


/**
 * Holds information about the current view, such as the size of the window,
 * and the coordinates zoomed in (and how zoomed in they are).
 */
typedef struct WINDOW
{
    double centrex;
    double centrey;
    double scale;
    int width;
    int height;
} WINDOW;

/** Translate from a point in the Mandelbrot set to a pixel, using the given window.
 *
 * @param win The window specifing the centre of zoom, scale, and window dimensions.
 * @param x, y Coordinates of a pixel in the window
 * @param px, py Pointers to where to save the point coordinates
 */
extern void pixel_to_point(WINDOW *win, int x, int y, double *px, double *py);

/** Translate from a pixel to a point in the Mandelbrot set to a pixel, using the given window.
 *
 * @param win The window specifing the centre of zoom, scale, and window dimensions.
 * @param px, py Coordinates of a point in the set
 * @param x, y Pointers to where to save the pixel coordinates.
 */
extern void point_to_pixel(WINDOW *win, double px, double py, int *x, int *y);


/**
 * Opaque fractal object, containing fractal-specific information.  Example
 * implementations are Mandelbrot and Julia fractals; they have distinct ways
 * of giving initial input to mfunc, and Julia fractals have a "Mandelbrot point".
 */
typedef struct FRACTAL FRACTAL;

/**
 * A callback that is defined by a fractal implementation.  It translates a
 * pixel to initial input, using the fractal's definition, including the window.
 *
 * @param fractal Fractal object to call this method on.
 * @param px,py Pixel coordinates, within the fractal's window.
 * @param zx,zy Pointers to "zero" point, the initial value of Z.
 * @param cx,cy Pointers to the coordinate, the initial value of C.
 */
typedef void GET_POINT(FRACTAL *fractal, int px, int py, double *zx, double *zy, double *cx, double *cy);


/**
 * Opaque type of a drawing mode object.
 */
typedef struct DRAWING DRAWING;


extern float do_pixel(int x, int y);
extern void set_pixel(int x, int y, float k);

extern int max_iterations;
extern int pixels_done;

extern char *status;

extern FRACTAL *mandelbrot_create(WINDOW *win);
extern void mandelbrot_get_point(FRACTAL *fractal, int px, int py, double *zx, double *zy, double *cx, double *cy);


extern DRAWING *simple_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc);
extern void simple_update(DRAWING *drawing);
extern void simple_destroy(DRAWING *drawing);

extern DRAWING *parallel_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc);
extern void parallel_update(DRAWING *drawing);
extern void parallel_destroy(DRAWING *drawing);

extern DRAWING *trace_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc);
extern void trace_update(DRAWING *drawing);
extern void trace_destroy(DRAWING *drawing);

extern DRAWING *iterative_create(WINDOW *window, FRACTAL *fractal, GET_POINT get_point, MFUNC *mfunc);
extern void iterative_update(DRAWING *drawing);
extern void iterative_destroy(DRAWING *drawing);

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
