#!/usr/bin/env sh
set -eu

export LC_ALL=C
export MPICH_CC=gcc

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MAX_CPUS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc)
PROCESSES=${PROCESSES:-"1 2 3 4 5 6 7 8 9 10 11 12"}
STRATEGIES=${STRATEGIES:-"block cyclic"}
RUNS=${RUNS:-5}
MODE_ARGS=${MODE_ARGS:-""}

if [ -z "${MODE_NAME:-}" ]; then
    case " $MODE_ARGS " in
        *" --seahorse "*) MODE_NAME=seahorse ;;
        *) MODE_NAME=padrao ;;
    esac
fi

RESULTS_FILE=${RESULTS_FILE:-"$ROOT_DIR/results/benchmark_mpi_${MODE_NAME}.csv"}
ENVIRONMENT_FILE=${ENVIRONMENT_FILE:-"$ROOT_DIR/results/environment_mpi.txt"}
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

make -C "$ROOT_DIR" -f Makefile.mpi all >/dev/null
mkdir -p "$(dirname -- "$RESULTS_FILE")"

{
    echo ""
    echo "=== Benchmark MPI: $MODE_NAME ==="
    echo "Data: $(date -Is)"
    echo "Sistema: $(uname -a)"
    echo "Compilador MPI: $(mpicc --version | sed -n '1p')"
    echo "Processos avaliados: $PROCESSES"
    echo "Estratégias: $STRATEGIES"
    if command -v lscpu >/dev/null 2>&1; then
        lscpu
    fi
} >> "$ENVIRONMENT_FILE"

printf 'modo,repeticao,processos,estrategia,t1_s,tp_s,t_max_s,t_min_s,t_avg_s,t_gather_s,t_reorder_s,t_comm_s,speedup,eficiencia,fator_balanceamento\n' > "$RESULTS_FILE"

for repetition in $(seq 1 "$RUNS"); do
    echo "Executando repeticao $repetition de $RUNS para $MODE_NAME..."
    (
        cd "$TEMP_DIR"
        # Execucao sequencial de referencia (MPI com 1 processo)
        mpirun -np 1 "$ROOT_DIR/mandelbrot_mpi" $MODE_ARGS > mpi_t1.log
        t1=$(awk -F': ' '/Tempo de calculo \(Tp\)/ { print $2 }' mpi_t1.log | awk '{print $1}')

        for p in $PROCESSES; do
            for strat in $STRATEGIES; do
                strat_flag="--$strat"
                mpirun -np "$p" "$ROOT_DIR/mandelbrot_mpi" $MODE_ARGS "$strat_flag" > mpi.log
                tp=$(awk -F': ' '/Tempo de calculo \(Tp\)/ { print $2 }' mpi.log | awk '{print $1}')
                t_gather=$(awk -F': ' '/Tempo de Coleta/ { print $2 }' mpi.log | awk '{print $1}')
                t_reorder=$(awk -F': ' '/Sobrecarga do Mestre/ { print $2 }' mpi.log | awk '{print $1}')
                t_comm=$(awk -F': ' '/Tempo Total de Comunicacao/ { print $2 }' mpi.log | awk '{print $1}')
                t_max=$(awk -F': ' '/Tempo Processo Max/ { print $2 }' mpi.log | awk '{print $1}')
                t_min=$(awk -F': ' '/Tempo Processo Min/ { print $2 }' mpi.log | awk '{print $1}')
                t_avg=$(awk -F': ' '/Tempo Processo Medio/ { print $2 }' mpi.log | awk '{print $1}')
                lbf=$(awk -F': ' '/Fator de Balanceamento/ { print $2 }' mpi.log | awk '{print $1}')
                speedup=$(awk -v t1="$t1" -v tp="$tp" 'BEGIN { printf "%.9f", t1 / tp }')
                efficiency=$(awk -v speedup="$speedup" -v p="$p" 'BEGIN { printf "%.9f", speedup / p }')

                printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
                    "$MODE_NAME" "$repetition" "$p" "$strat" \
                    "$t1" "$tp" "$t_max" "$t_min" "$t_avg" \
                    "$t_gather" "$t_reorder" "$t_comm" \
                    "$speedup" "$efficiency" "$lbf" >> "$RESULTS_FILE"
            done
        done
    )
done

echo "Resultados MPI gravados em $RESULTS_FILE"
echo "Ambiente registrado em $ENVIRONMENT_FILE"
