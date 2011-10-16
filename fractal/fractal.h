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

#endif
