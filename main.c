#include "mandelbrot.h"
#include <stdio.h>

int main() {
    int32_t *iterations = createMatrix();

    double re_step = (RE_MAX - RE_MIN) / (WIDTH - 1);
    double im_step = (IM_MAX - IM_MIN) / (HEIGHT - 1);

    int is_symmetric = 0;
    if (IM_MAX > 0 && (IM_MIN + IM_MAX) > -0.000001 && (IM_MIN + IM_MAX) < 0.000001) {
        is_symmetric = 1;
    }

    int limit_y = is_symmetric ? (HEIGHT / 2) : HEIGHT;

    for (int py = 0; py < limit_y; py++) {
        for (int px = 0; px < WIDTH; px++) {
            double c_re = RE_MIN + px * re_step;
            double c_im = IM_MIN + py * im_step;

            int iter = compute_pixel(c_re, c_im, MAX_ITER);
            iterations[py * WIDTH + px] = iter;

            if (is_symmetric) {
                int mirror_py = HEIGHT - 1 - py;
                iterations[mirror_py * WIDTH + px] = iter;
            }
        }
    }

    save_bin("mandelbrot.bin", iterations, WIDTH, HEIGHT);
    save_pgm("mandelbrot.pgm", iterations, WIDTH, HEIGHT, MAX_ITER);

    free(iterations);
    return 0;
}