#ifndef MANDELBROT_H
#define MANDELBROT_H

#include <stdint.h>

#define WIDTH 4096
#define HEIGHT 4096
#define MAX_ITER 1000

#define RE_MIN -2.0
#define RE_MAX 1.0
#define IM_MIN -1.5
#define IM_MAX 1.5

int compute_pixel(double c_re, double c_im, int max_iter);
void save_bin(const char *filename, int32_t *iterations, int width, int height);
void save_pgm(const char *filename, int32_t *iterations, int width, int height, int max_iter);

#endif