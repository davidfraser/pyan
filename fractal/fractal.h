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

#endif
