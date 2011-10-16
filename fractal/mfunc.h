#ifndef MFUNC_H
#define MFUNC_H


/** A pixel source is a callback that provides a new coordinate pair for the
 * Mandelbrot loop function to work on.  A module that uses mfunc_loop should
 * provide this callback as a way of telling mfunc_loop which pixels to work on.
 * The callback will be called with various values for slot (depending on the
 * mfunc_loop variant); when the callback returns a pixel to work on (by
 * returning non-zero), mfunc_loop will later call the pixel_output callback
 * with the same slot value.  The module should therefore use slot to track
 * its own information about pixels it gives to mfunc_loop.
 *
 * @param slot Slot of mfunc_loop that the pixel will be used for.
 * @param cx,cy Pointers to where the function should write the pixel coordinates.
 * @return Non-zero if a pixel was provided, or zero if there are no more pixels to work on.
 */
typedef int PIXEL_SOURCE(int slot, double *cx, double *cy);

/** Pixel output is a callback that the Mandelbrot loop function uses to
 * indicate a pixel computation has finished.  This callback is called for
 * every time next_pixel returns non-zero.  A module that uses mfunc_loop
 * should provide this callback as a way to get pixel results back from
 * mfunc_loop (for the pixels it provides to that function though the
 * next_pixel callback).
 *
 * @param slot Slot of pixel (same as slot used for original next_pixel call).
 * @param value Number of iterations before point was outside the set, or 0 if it remained in it after max_iterations.
 * @param cx,cy Last calculated position.
 */
typedef void PIXEL_OUTPUT(int slot, int value, double fx, double fy);

/** Original Mandelbrot function, that works on one pixel at a time.
 * @param cx,cy Coordinates of pixel to work on.
 * @param max_iterations Maximum number of iterations before assuming pixel is inside the set.
 * @param fx,fy Pointers to where last calculated position will be output.
 * @return The number of iterations run, or 0 if the pixel was still in the set after max_iterations.
 */
extern int mfunc(double cx, double cy, int max_iterations, double *fx, double *fy);

/** Mandelbrot loop function.  This function performs the same calculation as
 * mfunc, but has undergone a certain "inversion of control": it will call
 * the next_pixel callback to obtain a pixel coordinate pair to work on, and
 * it will call the output_pixel callback to return the result for that pixel.
 * These callbacks will be called continuously until next_pixel returns 0.
 *
 * The basic contract of mfunc_loop is:
 *   - Every time next_pixel returns non-zero for slot X, mfunc_loop will
 *     subsequently call output_pixel for slot X.
 *   - mfunc_loop will terminate when next_pixel has returned 0 for all slots.
 *
 * This version uses only one slot.  mfunc_simd has the same contract but uses up to 2 slots.
 *
 * @param max_iterations Maximum number of iterations, as in mloop.
 * @param next_pixel Callback for next_pixel.
 * @param output_pixel Callback for output_pixel.
 */
extern void mfunc_loop(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel);

/** SIMD version of Mandelbrot loop function.  Will use up to 2 slots.  Note
 * that it is the calling module's responsibility to ensure that the same
 * pixel is not worked on in both slots at the same time, unless the caller
 * can handle the consequences of that.
 * 
 * See mfunc_loop for further documentation.
 */
extern void mfunc_simd(int max_iterations, PIXEL_SOURCE next_pixel, PIXEL_OUTPUT output_pixel);

#endif
