#include "fractal.h"

#include <stdlib.h>

typedef struct FRACTAL
{
    WINDOW *window;
    double mandelbrot_x;
    double mandelbrot_y;
} FRACTAL;


FRACTAL *julia_create(WINDOW *win, double mandelbrot_x, double mandelbrot_y)
{
    FRACTAL *f = malloc(sizeof(FRACTAL));
    f->window = win;
    f->mandelbrot_x = mandelbrot_x;
    f->mandelbrot_y = mandelbrot_y;
    return f;
}


void julia_get_point(FRACTAL *fractal, int px, int py, double *zx, double *zy, double *cx, double *cy)
{
    pixel_to_point(fractal->window, px, py, zx, zy);
    *cx = fractal->mandelbrot_x;
    *cy = fractal->mandelbrot_y;
}


void julia_destroy(FRACTAL *fractal)
{
    free(fractal);
}
