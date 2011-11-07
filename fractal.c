#include "fractal.h"

void pixel_to_point(WINDOW *win, int x, int y, double *px, double *py)
{
    *px = (x - win->width/2.0)*win->scale + win->centrex;
    *py = (y - win->height/2.0)*win->scale + win->centrey;
}

void point_to_pixel(WINDOW *win, double px, double py, int *x, int *y)
{
    *x = (int) ((px - win->centrex)/win->scale + win->width/2.0);
    *y = (int) ((px - win->centrex)/win->scale + win->width/2.0);
}
