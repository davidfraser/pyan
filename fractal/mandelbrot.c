#include "fractal.h"

#include <stdlib.h>

typedef struct FRACTAL
{
    WINDOW *window;
} FRACTAL;


FRACTAL *mandelbrot_create(WINDOW *win)
{
    FRACTAL *f = malloc(sizeof(FRACTAL));
    f->window = win;
    return f;
}


void mandelbrot_get_point(FRACTAL *fractal, int px, int py, double *zx, double *zy, double *cx, double *cy)
{
    *zx = 0.0;
    *zy = 0.0;
    pixel_to_point(fractal->window, px, py, cx, cy);
}


void mandelbrot_destroy(FRACTAL *fractal)
{
    free(fractal);
}
