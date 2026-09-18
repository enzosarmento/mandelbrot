#ifndef MANDELBROT_H
#define MANDELBROT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>

#define WIDTH 4096
#define HEIGHT 4096
#define MAX_ITER 1000

#define RE_MIN -2.0
#define RE_MAX 1.0
#define IM_MIN -1.5
#define IM_MAX 1.5

#define SEAHORSE_CENTER_RE -0.743643887
#define SEAHORSE_CENTER_IM 0.131825904
#define SEAHORSE_WIDTH 0.003
#define SEAHORSE_RE_MIN (-0.743643887 - 0.0015)
#define SEAHORSE_RE_MAX (-0.743643887 + 0.0015)
#define SEAHORSE_IM_MIN (0.131825904 - 0.0015)
#define SEAHORSE_IM_MAX (0.131825904 + 0.0015)
#define SEAHORSE_MAX_ITER 5000

int32_t *createMatrix(int width, int height);
void configure_view(int width, int height, int is_seahorse,
                    double *re_min, double *re_max, double *im_min, double *im_max,
                    double *re_step, double *im_step, int *max_iter);
int compute_pixel(double c_re, double c_im, int max_iter);
int compute_pixel_at(int px, int py, double re_min, double im_min,
                     double re_step, double im_step, int max_iter);
void save_bin(const char *filename, int32_t *iterations, int width, int height);
void save_ppm(const char *filename, int32_t *iterations, int width, int height, int max_iter);

#endif
