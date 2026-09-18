#include "mandelbrot.h"

int main(int argc, char *argv[]) {
    int num_threads = 0;
    int width = WIDTH;
    int height = HEIGHT;
    int max_iter;
    int is_seahorse = 0;
    omp_sched_t schedule_kind = omp_sched_dynamic;
    const char *schedule_name = "dynamic";
    int chunk_size = 16;

    double re_min, re_max, im_min, im_max, re_step, im_step;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seahorse") == 0) {
            is_seahorse = 1;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) num_threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) {
            if (i + 1 < argc) width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) {
            if (i + 1 < argc) height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--schedule") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "A opcao --schedule exige static, dynamic ou guided.\n");
                return 1;
            }
            schedule_name = argv[++i];
            if (strcmp(schedule_name, "static") == 0) {
                schedule_kind = omp_sched_static;
            } else if (strcmp(schedule_name, "dynamic") == 0) {
                schedule_kind = omp_sched_dynamic;
            } else if (strcmp(schedule_name, "guided") == 0) {
                schedule_kind = omp_sched_guided;
            } else {
                fprintf(stderr, "Escalonamento invalido: %s. Use static, dynamic ou guided.\n", schedule_name);
                return 1;
            }
        } else if (strcmp(argv[i], "--chunk") == 0) {
            if (i + 1 >= argc || (chunk_size = atoi(argv[++i])) <= 0) {
                fprintf(stderr, "A opcao --chunk exige um inteiro positivo.\n");
                return 1;
            }
        } else if (argv[i][0] != '-' && num_threads == 0) {
            num_threads = atoi(argv[i]);
        }
    }

    if (width < 2 || height < 2) {
        fprintf(stderr, "Largura e altura devem ser maiores ou iguais a 2.\n");
        return 1;
    }

    if (num_threads <= 0) {
        num_threads = omp_get_max_threads();
    }
    omp_set_num_threads(num_threads);
    omp_set_schedule(schedule_kind, chunk_size);

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

    double *thread_times = (double *)calloc(num_threads, sizeof(double));
    long *thread_pixels = (long *)calloc(num_threads, sizeof(long));
    if (!thread_times || !thread_pixels) {
        fprintf(stderr, "Erro de alocacao para metricas de threads.\n");
        free(iterations);
        return 1;
    }

    double t_start = omp_get_wtime();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        double t_th_start = omp_get_wtime();
        long local_px = 0;

        #pragma omp for schedule(runtime) nowait
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
            local_px += width;
        }

        thread_times[tid] = omp_get_wtime() - t_th_start;
        thread_pixels[tid] = local_px;
    }

    double t_calc = omp_get_wtime() - t_start;

    double t_max = thread_times[0];
    double t_min = thread_times[0];
    double t_sum = 0.0;

    for (int t = 0; t < num_threads; t++) {
        if (thread_times[t] > t_max) t_max = thread_times[t];
        if (thread_times[t] < t_min) t_min = thread_times[t];
        t_sum += thread_times[t];
    }
    double t_avg = t_sum / num_threads;
    double lbf = (t_max > 0.0) ? (t_avg / t_max) : 1.0;
    double imbalance_ratio = (t_avg > 0.0) ? (t_max / t_avg) : 1.0;

    printf("=== RESUMO DA EXECUCAO (OPENMP) ===\n");
    printf("Modo: %s\n", is_seahorse ? "Vale dos Cavalos-Marinhos (Estresse)" : "Padrao (Vista Completa)");
    printf("Resolucao: %dx%d\n", width, height);
    printf("Max iteracoes: %d\n", max_iter);
    printf("Simetria explorada: %s\n", is_symmetric ? "Sim" : "Nao");
    printf("Threads: %d\n", num_threads);
    printf("Politica de escalonamento: %s, %d\n", schedule_name, chunk_size);
    printf("Tempo de calculo (Tp): %.6f segundos\n", t_calc);
    printf("--- BALANCEAMENTO DE CARGA ---\n");
    printf("Tempo Thread Max (T_max): %.6f segundos\n", t_max);
    printf("Tempo Thread Min (T_min): %.6f segundos\n", t_min);
    printf("Tempo Thread Medio (T_avg): %.6f segundos\n", t_avg);
    printf("Fator de Balanceamento (T_avg / T_max): %.4f (%.2f%%)\n", lbf, lbf * 100.0);
    printf("Razao de Desbalanceamento (T_max / T_avg): %.4f\n", imbalance_ratio);
    if (t_min > 0.0) {
        printf("Razao T_max / T_min: %.4f\n", t_max / t_min);
    }

    printf("--- DETALHES POR THREAD ---\n");
    for (int t = 0; t < num_threads; t++) {
        printf("  Thread %2d: %.6f s (%5.1f%% do T_max) | %ld pixels\n",
               t, thread_times[t], (t_max > 0 ? (thread_times[t] / t_max) * 100.0 : 0.0), thread_pixels[t]);
    }

    double t_io_start = omp_get_wtime();

    save_bin("mandelbrot_openmp.bin", iterations, width, height);
    save_ppm("mandelbrot_openmp.ppm", iterations, width, height, max_iter);

    double t_io = omp_get_wtime() - t_io_start;
    printf("Tempo de E/S: %.6f segundos\n", t_io);

    free(thread_times);
    free(thread_pixels);
    free(iterations);

    return 0;
}
