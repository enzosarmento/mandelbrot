#!/usr/bin/env sh
# Escalabilidade fraca MPI: cada caso aumenta a resolução junto com P.
set -eu

export LC_ALL=C
export MPICH_CC=gcc

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
RUNS=${RUNS:-3}
STRATEGIES=${STRATEGIES:-"block cyclic"}
MODE_ARGS=${MODE_ARGS:-""}
MODE_NAME=${MODE_NAME:-}
WEAK_CASES=${WEAK_CASES:-"4096x4096:1 8192x8192:4 16384x16384:16"}
RESULTS_FILE=${RESULTS_FILE:-"$ROOT_DIR/results/benchmark_mpi_escalabilidade_fraca.csv"}
ENVIRONMENT_FILE=${ENVIRONMENT_FILE:-"$ROOT_DIR/results/environment_mpi.txt"}
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

if [ -z "$MODE_NAME" ]; then
    case " $MODE_ARGS " in
        *" --seahorse "*) MODE_NAME=seahorse ;;
        *) MODE_NAME=padrao ;;
    esac
fi
make -C "$ROOT_DIR" -f Makefile.mpi all >/dev/null
mkdir -p "$(dirname -- "$RESULTS_FILE")"
printf 'largura,altura,modo,repeticao,processos,estrategia,t_base_s,tp_s,eficiencia_fraca,t_gather_s,t_reorder_s,t_comm_s,t_max_s,t_min_s,t_avg_s,fator_balanceamento\n' > "$RESULTS_FILE"

{
    echo ""
    echo "=== Benchmark MPI: escalabilidade fraca ==="
    echo "Data: $(date -Is)"
    echo "Casos (larguraxaltura:processos): $WEAK_CASES"
    echo "Estratégias: $STRATEGIES"
    echo "Definição: eficiencia_fraca = T_base(4096x4096,P=1) / Tp."
} >> "$ENVIRONMENT_FILE"

for repetition in $(seq 1 "$RUNS"); do
    (
        cd "$TEMP_DIR"
        set -- $WEAK_CASES
        base_case=$1
        base_resolution=${base_case%%:*}
        base_width=${base_resolution%x*}
        base_height=${base_resolution#*x}
        mpirun -np 1 "$ROOT_DIR/bin/mandelbrot_mpi" $MODE_ARGS --width "$base_width" --height "$base_height" --block > mpi_base.log
        t_base=$(awk -F': ' '/Tempo de calculo \(Tp\)/ { print $2 }' mpi_base.log | awk '{print $1}')

        for weak_case in $WEAK_CASES; do
            resolution=${weak_case%%:*}
            processes=${weak_case#*:}
            width=${resolution%x*}
            height=${resolution#*x}
            case "$width:$height:$processes" in *[!0-9:]*|*::*|:*|*:) echo "ERRO: WEAK_CASES usa larguraxaltura:processos." >&2; exit 1 ;; esac

            for strategy in $STRATEGIES; do
                mpirun -np "$processes" "$ROOT_DIR/bin/mandelbrot_mpi" $MODE_ARGS --width "$width" --height "$height" "--$strategy" > mpi.log
                tp=$(awk -F': ' '/Tempo de calculo \(Tp\)/ { print $2 }' mpi.log | awk '{print $1}')
                gather=$(awk -F': ' '/Tempo de Coleta/ { print $2 }' mpi.log | awk '{print $1}')
                reorder=$(awk -F': ' '/Sobrecarga do Mestre/ { print $2 }' mpi.log | awk '{print $1}')
                comm=$(awk -F': ' '/Tempo Total de Comunicacao/ { print $2 }' mpi.log | awk '{print $1}')
                tmax=$(awk -F': ' '/Tempo Processo Max/ { print $2 }' mpi.log | awk '{print $1}')
                tmin=$(awk -F': ' '/Tempo Processo Min/ { print $2 }' mpi.log | awk '{print $1}')
                tavg=$(awk -F': ' '/Tempo Processo Medio/ { print $2 }' mpi.log | awk '{print $1}')
                fbc=$(awk -F': ' '/Fator de Balanceamento/ { print $2 }' mpi.log | awk '{print $1}')
                efficiency=$(awk -v base="$t_base" -v time="$tp" 'BEGIN { printf "%.9f", base / time }')
                printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
                    "$width" "$height" "$MODE_NAME" "$repetition" "$processes" "$strategy" \
                    "$t_base" "$tp" "$efficiency" "$gather" "$reorder" "$comm" \
                    "$tmax" "$tmin" "$tavg" "$fbc" >> "$RESULTS_FILE"
            done
        done
    )
done

echo "Escalabilidade fraca MPI gravada em $RESULTS_FILE"
