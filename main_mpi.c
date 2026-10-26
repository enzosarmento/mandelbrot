#include <mpi.h>
#include "mandelbrot.h"

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int width = WIDTH;
    int height = HEIGHT;
    int is_seahorse = 0;
    int is_cyclic = 0;
    int status_code = 0;

    /*
     * Processo raiz (rank 0) realiza a leitura e validacao dos argumentos de entrada,
     */
    if (rank == 0) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seahorse") == 0) {
                is_seahorse = 1;
            } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--width") == 0) {
                if (i + 1 < argc) width = atoi(argv[++i]);
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--height") == 0) {
                if (i + 1 < argc) height = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--cyclic") == 0 || strcmp(argv[i], "-c") == 0) {
                is_cyclic = 1;
            } else if (strcmp(argv[i], "--block") == 0 || strcmp(argv[i], "-b") == 0) {
                is_cyclic = 0;
            } else if (strcmp(argv[i], "--help") == 0) {
                printf("Uso: mpirun -np <P> %s [opcoes]\n", argv[0]);
                printf("Opcoes:\n");
                printf("  -s, --seahorse     Aproxima a regiao do Vale dos Cavalos-Marinhos (5000 iteracoes)\n");
                printf("  -w, --width <N>    Largura da imagem (padrao: %d)\n", WIDTH);
                printf("  -h, --height <N>   Altura da imagem (padrao: %d)\n", HEIGHT);
                printf("  -b, --block        Distribuicao em faixas contiguas de linhas (padrao - Scatter/Gather)\n");
                printf("  -c, --cyclic       Distribuicao ciclica (round-robin) de linhas para balanceamento de carga\n");
                status_code = 2;
                break;
            } else if (argv[i][0] != '-') {
                /* Argumento posicional sem flag desconsiderado */
            }
        }

        if (status_code == 0 && (width < 2 || height < 2)) {
            fprintf(stderr, "Largura e altura devem ser maiores ou iguais a 2.\n");
            status_code = 1;
        }
    }

    /*
     * Difunde o status da validacao para todos os processos
     */
    MPI_Bcast(&status_code, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (status_code != 0) {
        MPI_Finalize();
        return (status_code == 2) ? 0 : 1;
    }

    /*
     * Difusao dos parametros de configuracao para todos os processos via MPI_Bcast.
     */
    int config[4];
    if (rank == 0) {
        config[0] = width;
        config[1] = height;
        config[2] = is_seahorse;
        config[3] = is_cyclic;
    }
    MPI_Bcast(config, 4, MPI_INT, 0, MPI_COMM_WORLD);
    width = config[0];
    height = config[1];
    is_seahorse = config[2];
    is_cyclic = config[3];

    /*
     * Configuracao dos limites do plano complexo e iteracoes maximas.
     * Como os parametros de entrada sao identicos em todos os ranks,
     * as variaveis matematicas sao perfeitamente sincronizadas.
     */
    double re_min, re_max, im_min, im_max, re_step, im_step;
    int max_iter;
    configure_view(width, height, is_seahorse, &re_min, &re_max, &im_min, &im_max,
                   &re_step, &im_step, &max_iter);

    /*
     * Decomposicao de dominio em faixas de linhas (horizontal slices).
     * Conforme abordado no material do Filtro Gaussiano:
     * - VERSAO 2: blocos iguais de N/P linhas (quando height e divisivel por size)
     * - VERSAO 3: blocos de tamanhos diferentes com vetores de contagens e deslocamentos (Scatterv / Gatherv)
     */
    int *rows_per_proc = (int *)malloc((size_t)size * sizeof(int));
    int *start_rows = (int *)malloc((size_t)size * sizeof(int));
    int *recvcounts = (int *)malloc((size_t)size * sizeof(int));
    int *displs = (int *)malloc((size_t)size * sizeof(int));

    if (!rows_per_proc || !start_rows || !recvcounts || !displs) {
        fprintf(stderr, "[Rank %d] Falha de alocacao para estruturas de particao.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int base_rows = height / size;
    int remainder = height % size;

    if (!is_cyclic) {
        /*
         * Faixas de linhas contiguas:
         * Se remainder > 0, os primeiros 'remainder' processos recebem 1 linha extra.
         */
        for (int r = 0; r < size; r++) {
            rows_per_proc[r] = base_rows + (r < remainder ? 1 : 0);
            start_rows[r] = r * base_rows + (r < remainder ? r : remainder);
            recvcounts[r] = rows_per_proc[r] * width;
            displs[r] = start_rows[r] * width;
        }
    } else {
        /*
         * Distribuicao ciclica (round-robin por linhas).
         * Cada processo r calcula as linhas: r, r + size, r + 2*size, ...
         */
        for (int r = 0; r < size; r++) {
            rows_per_proc[r] = (height - r + size - 1) / size;
            if (rows_per_proc[r] < 0) rows_per_proc[r] = 0;
            start_rows[r] = r;
            recvcounts[r] = rows_per_proc[r] * width;
            displs[r] = (r == 0) ? 0 : displs[r - 1] + recvcounts[r - 1];
        }
    }

    int local_rows = rows_per_proc[rank];
    int start_row = start_rows[rank];

    /* Alocacao do buffer local para os pixels computados por este processo */
    int32_t *local_iterations = NULL;
    if (local_rows > 0) {
        local_iterations = (int32_t *)malloc((size_t)local_rows * width * sizeof(int32_t));
        if (!local_iterations) {
            fprintf(stderr, "[Rank %d] Falha de alocacao do buffer local.\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    /* O processo raiz aloca a matriz global completa */
    int32_t *iterations = NULL;
    int32_t *gather_buf = NULL;
    if (rank == 0) {
        iterations = createMatrix(width, height);
        if (!iterations) {
            fprintf(stderr, "[Rank 0] Falha ao alocar matriz global.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (is_cyclic) {
            gather_buf = (int32_t *)malloc((size_t)width * height * sizeof(int32_t));
            if (!gather_buf) {
                fprintf(stderr, "[Rank 0] Falha ao alocar buffer auxiliar ciclico.\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }

    /*
     * Sincronizacao antes do inicio do calculo para medicao precisa do tempo paralelo Tp.
     */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_calc_start = MPI_Wtime();

    /* Medicao do tempo individual de computacao deste processo */
    double t_local_start = MPI_Wtime();

    /*
     * Calculo dos pixels do Mandelbrot:
     * Nesta versao MPI, nao e utilizada a otimizacao de simetria do problema;
     * todas as linhas designadas sao calculadas diretamente por compute_pixel_at.
     */
    if (!is_cyclic) {
        for (int i = 0; i < local_rows; i++) {
            int py = start_row + i;
            for (int px = 0; px < width; px++) {
                local_iterations[i * width + px] = compute_pixel_at(px, py, re_min, im_min,
                                                                   re_step, im_step, max_iter);
            }
        }
    } else {
        for (int i = 0; i < local_rows; i++) {
            int py = rank + i * size;
            for (int px = 0; px < width; px++) {
                local_iterations[i * width + px] = compute_pixel_at(px, py, re_min, im_min,
                                                                   re_step, im_step, max_iter);
            }
        }
    }

    double t_local_calc = MPI_Wtime() - t_local_start;

    /*
     * Coleta dos resultados no processo raiz e medicao do custo de comunicacao:
     * - Sincronizamos todos os processos com uma barreira para que o tempo medido no
     *   Gather/Gatherv represente estritamente o tempo de transmissao/coleta de dados,
     *   e nao o tempo de espera ociosa decorrente do desbalanceamento computacional.
     * - Cronometramos a coleta coletiva (t_gather).
     * - Na distribuicao ciclica, cronometramos a sobrecarga do processo raiz (t_reorder)
     *   ao desentrelaçar as linhas via memcpy.
     */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_gather_start = MPI_Wtime();

    if (!is_cyclic) {
        if (remainder == 0) {
            MPI_Gather(local_iterations,
                       local_rows * width,
                       MPI_INT,
                       iterations,
                       local_rows * width,
                       MPI_INT,
                       0,
                       MPI_COMM_WORLD);
        } else {
            MPI_Gatherv(local_iterations,
                        local_rows * width,
                        MPI_INT,
                        iterations,
                        recvcounts,
                        displs,
                        MPI_INT,
                        0,
                        MPI_COMM_WORLD);
        }
    } else {
        MPI_Gatherv(local_iterations,
                    local_rows * width,
                    MPI_INT,
                    gather_buf,
                    recvcounts,
                    displs,
                    MPI_INT,
                    0,
                    MPI_COMM_WORLD);
    }
    double t_gather_local = MPI_Wtime() - t_gather_start;

    double t_reorder = 0.0;
    if (is_cyclic && rank == 0) {
        double t_reorder_start = MPI_Wtime();
        for (int r = 0; r < size; r++) {
            int r_rows = rows_per_proc[r];
            int32_t *r_data = gather_buf + displs[r];
            for (int i = 0; i < r_rows; i++) {
                int py = r + i * size;
                memcpy(iterations + (size_t)py * width,
                       r_data + (size_t)i * width,
                       (size_t)width * sizeof(int32_t));
            }
        }
        free(gather_buf);
        t_reorder = MPI_Wtime() - t_reorder_start;
    }

    /*
     * Fecha a janela de T(n,p) somente depois de todos os processos terem
     * terminado. Assim, Tp mede do inicio comum ao termino do ultimo processo,
     * como definido na Aula 03. A E/S dos arquivos fica deliberadamente fora
     * dessa janela: ela e serial e nao faz parte do nucleo paralelo avaliado.
     */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_calc_local = MPI_Wtime() - t_calc_start;
    double t_calc = 0.0;
    double t_gather = 0.0;
    MPI_Reduce(&t_calc_local, &t_calc, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_gather_local, &t_gather, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    double t_comm_total = t_gather + t_reorder;

    /*
     * Metricas de desempenho e balanceamento de carga:
     * - Coleta dos tempos e contagens de pixels de cada processo via MPI_Gather.
     * - Uso de MPI_Reduce (Aula 11) para reducoes globais (maximo, minimo, soma).
     */
    long local_px = (long)local_rows * width;

    double *proc_times = NULL;
    long *proc_pixels = NULL;
    if (rank == 0) {
        proc_times = (double *)malloc((size_t)size * sizeof(double));
        proc_pixels = (long *)malloc((size_t)size * sizeof(long));
    }

    MPI_Gather(&t_local_calc, 1, MPI_DOUBLE, proc_times, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_px, 1, MPI_LONG, proc_pixels, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    double t_max = 0.0, t_min = 0.0, t_sum = 0.0;
    MPI_Reduce(&t_local_calc, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_local_calc, &t_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_local_calc, &t_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double t_avg = t_sum / size;
        double lbf = (t_max > 0.0) ? ((t_max - t_min) / t_max) : 0.0;
        double imbalance_ratio = (t_avg > 0.0) ? (t_max / t_avg) : 1.0;

        printf("=== RESUMO DA EXECUCAO (MPI) ===\n");
        printf("Modo: %s\n", is_seahorse ? "Vale dos Cavalos-Marinhos (Estresse)" : "Padrao (Vista Completa)");
        printf("Resolucao: %dx%d\n", width, height);
        printf("Max iteracoes: %d\n", max_iter);
        printf("Simetria explorada: Nao\n");
        printf("Processos MPI: %d\n", size);
        printf("Estrategia de divisao: %s\n",
               is_cyclic ? "Ciclica (round-robin por linhas)" :
               (remainder == 0 ? "Faixas contiguas de linhas (MPI_Gather - blocos iguais)" :
                                 "Faixas contiguas de linhas (MPI_Gatherv - blocos variaveis)"));
        printf("Tempo de calculo (Tp): %.6f segundos\n", t_calc);
        printf("--- CUSTO DE COMUNICACAO E SOBRECARGA ---\n");
        printf("Tempo de Coleta (Gather/Gatherv): %.6f segundos\n", t_gather);
        printf("Sobrecarga do Mestre (Rearranjo): %.6f segundos\n", t_reorder);
        printf("Tempo Total de Comunicacao: %.6f segundos (%.2f%% do Tp)\n",
               t_comm_total, (t_calc > 0.0 ? (t_comm_total / t_calc) * 100.0 : 0.0));
        printf("--- BALANCEAMENTO DE CARGA ---\n");
        printf("Tempo Processo Max (T_max): %.6f segundos\n", t_max);
        printf("Tempo Processo Min (T_min): %.6f segundos\n", t_min);
        printf("Tempo Processo Medio (T_avg): %.6f segundos\n", t_avg);
        printf("Fator de Balanceamento (FLB = (T_max - T_min) / T_max): %.4f (%.2f%%; 0%% e ideal)\n",
               lbf, lbf * 100.0);
        printf("Razao de Desbalanceamento (T_max / T_avg): %.4f\n", imbalance_ratio);
        if (t_min > 0.0) {
            printf("Razao T_max / T_min: %.4f\n", t_max / t_min);
        }

        printf("--- DETALHES POR PROCESSO ---\n");
        for (int r = 0; r < size; r++) {
            if (!is_cyclic) {
                if (rows_per_proc[r] > 0) {
                    printf("  Processo %2d: %.6f s (%5.1f%% do T_max) | %ld pixels (linhas %d a %d)\n",
                           r, proc_times[r], (t_max > 0 ? (proc_times[r] / t_max) * 100.0 : 0.0),
                           proc_pixels[r], start_rows[r], start_rows[r] + rows_per_proc[r] - 1);
                } else {
                    printf("  Processo %2d: %.6f s (%5.1f%% do T_max) | 0 pixels (sem linhas alocadas)\n",
                           r, proc_times[r], (t_max > 0 ? (proc_times[r] / t_max) * 100.0 : 0.0));
                }
            } else {
                printf("  Processo %2d: %.6f s (%5.1f%% do T_max) | %ld pixels (passo %d a partir da linha %d)\n",
                       r, proc_times[r], (t_max > 0 ? (proc_times[r] / t_max) * 100.0 : 0.0),
                       proc_pixels[r], size, r);
            }
        }

        double t_io_start = MPI_Wtime();
        save_bin("mandelbrot_mpi.bin", iterations, width, height);
        save_ppm("mandelbrot_mpi.ppm", iterations, width, height, max_iter);
        double t_io = MPI_Wtime() - t_io_start;
        printf("Tempo de E/S: %.6f segundos\n", t_io);

        free(proc_times);
        free(proc_pixels);
        free(iterations);
    }

    if (local_iterations) free(local_iterations);
    free(rows_per_proc);
    free(start_rows);
    free(recvcounts);
    free(displs);

    MPI_Finalize();
    return 0;
}
