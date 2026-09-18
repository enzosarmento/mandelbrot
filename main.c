#include "mandelbrot.h"

int main(int argc, char *argv[]) {
    int width = WIDTH;
    int height = HEIGHT;
    int is_seahorse = 0;

    double re_min, re_max, im_min, im_max, re_step, im_step;
    int max_iter;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seahorse") == 0) {
            is_seahorse = 1;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) {
            if (i + 1 < argc) width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) {
            if (i + 1 < argc) height = atoi(argv[++i]);
        }
    }

    if (width < 2 || height < 2) {
        fprintf(stderr, "Largura e altura devem ser maiores ou iguais a 2.\n");
        return 1;
    }

    configure_view(width, height, is_seahorse, &re_min, &re_max, &im_min, &im_max,
                   &re_step, &im_step, &max_iter);

    int32_t *iterations = createMatrix(width, height);
    if (!iterations) {
        return 1;
    }

    int is_symmetric = 0;
    if (im_max > 0 && (im_min + im_max) > -0.000001 && (im_min + im_max) < 0.000001) {
        is_symmetric = 1;
    }

    int limit_y = is_symmetric ? ((height + 1) / 2) : height;

    double t_start = omp_get_wtime();

    for (int py = 0; py < limit_y; py++) {
        for (int px = 0; px < width; px++) {
            int iter = compute_pixel_at(px, py, re_min, im_min,
                                        re_step, im_step, max_iter);
            iterations[py * width + px] = iter;

            if (is_symmetric) {
                int mirror_py = height - 1 - py;
                iterations[mirror_py * width + px] = iter;
            }
        }
    }

    double t_calc = omp_get_wtime() - t_start;

    printf("=== RESUMO DA EXECUCAO (SEQUENCIAL) ===\n");
    printf("Modo: %s\n", is_seahorse ? "Vale dos Cavalos-Marinhos (Estresse)" : "Padrao (Vista Completa)");
    printf("Resolucao: %dx%d\n", width, height);
    printf("Max iteracoes: %d\n", max_iter);
    printf("Simetria explorada: %s\n", is_symmetric ? "Sim" : "Nao");
    printf("Tempo de calculo (T1): %.6f segundos\n", t_calc);

    double t_io_start = omp_get_wtime();

    save_bin("mandelbrot.bin", iterations, width, height);
    save_ppm("mandelbrot.ppm", iterations, width, height, max_iter);

    double t_io = omp_get_wtime() - t_io_start;
    printf("Tempo de E/S: %.6f segundos\n", t_io);

    free(iterations);
    return 0;
}
