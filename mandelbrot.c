#include "mandelbrot.h"
#include <math.h>
#include <stdio.h>


int32_t *createMatrix(int width, int height) {
    int32_t *iterations = (int32_t *)malloc((size_t)width * height * sizeof(int32_t));
    if (!iterations) {
        fprintf(stderr, "Erro de alocacao de memoria.\n");
        return NULL;
    }
    return iterations;
}

void configure_view(int width, int height, int is_seahorse,
                    double *re_min, double *re_max, double *im_min, double *im_max,
                    double *re_step, double *im_step, int *max_iter) {
    if (is_seahorse) {
        double im_span = SEAHORSE_WIDTH * (double)height / width;
        *re_min = SEAHORSE_CENTER_RE - SEAHORSE_WIDTH / 2.0;
        *re_max = SEAHORSE_CENTER_RE + SEAHORSE_WIDTH / 2.0;
        *im_min = SEAHORSE_CENTER_IM - im_span / 2.0;
        *im_max = SEAHORSE_CENTER_IM + im_span / 2.0;
        *max_iter = SEAHORSE_MAX_ITER;
    } else {
        *re_min = RE_MIN;
        *re_max = RE_MAX;
        *im_min = IM_MIN;
        *im_max = IM_MAX;
        *max_iter = MAX_ITER;
    }

    *re_step = (*re_max - *re_min) / (width - 1);
    *im_step = (*im_max - *im_min) / (height - 1);
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

/* Mantém o mesmo arredondamento no mapeamento para as versões sequencial e OpenMP. */
int compute_pixel_at(int px, int py, double re_min, double im_min,
                     double re_step, double im_step, int max_iter) {
    double c_re = re_min + (double)px * re_step;
    double c_im = im_min + (double)py * im_step;
    return compute_pixel(c_re, c_im, max_iter);
}

void save_bin(const char *filename, int32_t *iterations, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (f != NULL) {
        fwrite(iterations, sizeof(int32_t), (size_t)width * height, f);
        fclose(f);
        printf("Arquivo %s gerado.\n", filename);
    }
}

/* Paleta em tons de azul, com contraste suave para revelar os detalhes do zoom. */
static void color_from_iteration(int iter, int max_iter,
                                 unsigned char *red, unsigned char *green,
                                 unsigned char *blue) {
    if (iter >= max_iter) {
        *red = 0;
        *green = 0;
        *blue = 0;
        return;
    }

    /* A escala logaritmica destaca as variacoes nas regioes de maior iteracao. */
    double value = log((double)iter + 1.0) / log((double)max_iter + 1.0);
    double scaled = value * 4.0;
    int band = (int)scaled;
    double fraction = scaled - band;
    static const unsigned char palette[][3] = {
        {  7,  27,  51}, /* azul-marinho */
        { 18,  58,  90}, /* azul profundo */
        { 45, 103, 142}, /* azul acinzentado */
        {117, 175, 199}, /* azul claro */
        {210, 230, 238}  /* azul muito claro */
    };

    if (band > 3) {
        band = 3;
        fraction = 1.0;
    }

    *red = (unsigned char)(palette[band][0] +
                           fraction * (palette[band + 1][0] - palette[band][0]));
    *green = (unsigned char)(palette[band][1] +
                             fraction * (palette[band + 1][1] - palette[band][1]));
    *blue = (unsigned char)(palette[band][2] +
                            fraction * (palette[band + 1][2] - palette[band][2]));
}

void save_ppm(const char *filename, int32_t *iterations, int width, int height, int max_iter) {
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        return;
    }

    fprintf(f, "P6\n%d %d\n255\n", width, height);
    size_t total = (size_t)width * height;
    for (size_t i = 0; i < total; i++) {
        unsigned char pixel[3];
        color_from_iteration(iterations[i], max_iter, &pixel[0], &pixel[1], &pixel[2]);
        fwrite(pixel, sizeof(pixel), 1, f);
    }

    fclose(f);
    printf("Arquivo colorido %s gerado.\n", filename);
}
