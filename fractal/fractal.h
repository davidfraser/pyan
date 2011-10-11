#ifndef FRACTAL_H
#define FRACTAL_H

typedef int PIXEL_SOURCE(int slot, double *cx, double *cy);
typedef void PIXEL_OUTPUT(int slot, int value, double fx, double fy);

extern int mfunc(double cx, double cy, int max_iterations, double *fx, double *fy);
extern void mfunc_loop(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel);
#if WIN32
extern void mfunc_simd(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel);
#else
#define mfunc_simd mfunc_loop
#endif

extern float do_pixel(int x, int y);
extern void set_pixel(int x, int y, float k);

extern double centrex, centrey;
extern double scale;
extern int max_iterations;
extern int pixels_done;

extern char *status;

#endif
