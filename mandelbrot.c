#include "mandelbrot.h"
#include <stdio.h>


int32_t *createMatrix() {
    int32_t *iterations = (int32_t *)malloc(WIDTH * HEIGHT * sizeof(int32_t));
    if (!iterations) {
        fprintf(stderr, "Erro de alocacao de memoria.\n");
        return NULL;
    }
    return iterations;
}

int compute_pixel(double c_re, double c_im, int max_iter) {
    // Otimização do Cardióide e Bulbo
    double q = (c_re - 0.25) * (c_re - 0.25) + c_im * c_im;
    if (q * (q + (c_re - 0.25)) <= 0.25 * c_im * c_im || 
       (c_re + 1.0) * (c_re + 1.0) + c_im * c_im <= 0.0625) {
        return max_iter;
    }

    double z_re = 0.0, z_im = 0.0;
    double z_re2 = 0.0, z_im2 = 0.0;
    int iter = 0;

    while ((z_re2 + z_im2) <= 4.0 && iter < max_iter) {
        double next_z_im = 2.0 * z_re * z_im + c_im;
        z_re = z_re2 - z_im2 + c_re;
        z_im = next_z_im;
        
        z_re2 = z_re * z_re;
        z_im2 = z_im * z_im;
        iter++;
    }

    return iter;
}

void save_bin(const char *filename, int32_t *iterations, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (f != NULL) {
        fwrite(iterations, sizeof(int32_t), width * height, f);
        fclose(f);
        printf("Arquivo %s gerado.\n", filename);
    }
}

void save_pgm(const char *filename, int32_t *iterations, int width, int height, int max_iter) {
    FILE *f = fopen(filename, "w");
    if (f != NULL) {
        fprintf(f, "P2\n%d %d\n%d\n", width, height, max_iter);
        for (int i = 0; i < width * height; i++) {
            fprintf(f, "%d ", iterations[i]);
            if ((i + 1) % width == 0) fprintf(f, "\n");
        }
        fclose(f);
        printf("Arquivo %s gerado.\n", filename);
    }
}