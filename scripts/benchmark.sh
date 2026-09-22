#!/usr/bin/env sh
set -eu

# CSV usa vírgula como separador; força ponto como separador decimal.
export LC_ALL=C

# Exemplos:
#   RUNS=5 ./scripts/benchmark.sh
#   MODE_ARGS="--seahorse" RUNS=5 ./scripts/benchmark.sh
#   BENCHMARK_TYPE=weak RUNS=5 ./scripts/benchmark.sh
#   BENCHMARK_TYPE=weak WEAK_MODES="seahorse" RUNS=5 ./scripts/benchmark.sh
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MAX_THREADS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc)
if [ "$MAX_THREADS" -lt 2 ]; then
    echo "ERRO: o benchmark requer pelo menos 2 threads disponíveis." >&2
    exit 1
fi
BENCHMARK_TYPE=${BENCHMARK_TYPE:-strong}
ENVIRONMENT_FILE=${ENVIRONMENT_FILE:-"$ROOT_DIR/results/environment.txt"}

if [ "$BENCHMARK_TYPE" = "weak" ]; then
    RUNS=${RUNS:-3}
    WEAK_SCHEDULES=${WEAK_SCHEDULES:-dynamic}
    WEAK_CHUNKS=${WEAK_CHUNKS:-1}
    RESULTS_FILE=${RESULTS_FILE:-"$ROOT_DIR/results/benchmark_escalabilidade_fraca.csv"}
    TEMP_DIR=$(mktemp -d)
    trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

    if [ -z "${WEAK_MODES:-}" ]; then
        if [ -n "${MODE_NAME:-}" ]; then
            WEAK_MODES="$MODE_NAME"
        elif [ -n "${MODE_ARGS:-}" ]; then
            case " $MODE_ARGS " in
                *" --seahorse "*) WEAK_MODES="seahorse" ;;
                *) WEAK_MODES="padrao" ;;
            esac
        else
            WEAK_MODES="padrao seahorse"
        fi
    fi

    if [ -z "${WEAK_CASES:-}" ]; then
        threads_8192=8
        threads_16384=32
        [ "$threads_8192" -gt "$MAX_THREADS" ] && threads_8192=$MAX_THREADS
        [ "$threads_16384" -gt "$MAX_THREADS" ] && threads_16384=$MAX_THREADS
        WEAK_CASES="4096:2 8192:$threads_8192 16384:$threads_16384"
    fi

    make -C "$ROOT_DIR" -f Makefile.seq all >/dev/null
    make -C "$ROOT_DIR" -f Makefile.openmp all >/dev/null
    mkdir -p "$(dirname -- "$RESULTS_FILE")"
    printf 'largura,altura,modo,repeticao,threads,escalonamento,chunk,t1_s,tp_s,speedup,eficiencia,fator_balanceamento\n' > "$RESULTS_FILE"
    {
        echo ""
        echo "=== Benchmark: escalabilidade fraca ==="
        echo "Data: $(date -Is)"
        echo "Modos: $WEAK_MODES"
        echo "Casos (resolução:threads): $WEAK_CASES"
        echo "Políticas: $WEAK_SCHEDULES"
        echo "Chunks: $WEAK_CHUNKS"
        echo "Máximo de threads disponível: $MAX_THREADS"
        echo "Observação: casos limitados pelo hardware não preservam exatamente o trabalho por thread."
    } >> "$ENVIRONMENT_FILE"

    for mode in $WEAK_MODES; do
        case "$mode" in
            padrao)
                mode_flag=""
                ;;
            seahorse)
                mode_flag="--seahorse"
                ;;
            *)
                echo "ERRO: modo de escalabilidade fraca desconhecido: $mode (use 'padrao' ou 'seahorse')." >&2
                exit 1
                ;;
        esac

        for weak_case in $WEAK_CASES; do
            resolution=${weak_case%%:*}
            threads=${weak_case#*:}
            case "$resolution:$threads" in
                *[!0-9:]*|*:*:*)
                    echo "ERRO: WEAK_CASES deve usar o formato resolução:threads." >&2
                    exit 1
                    ;;
            esac
            if [ "$resolution" -lt 2 ] || [ "$threads" -lt 2 ] || [ "$threads" -gt "$MAX_THREADS" ]; then
                echo "ERRO: cada caso deve ter resolução >= 2 e threads entre 2 e $MAX_THREADS." >&2
                exit 1
            fi

            echo "Executando escalabilidade fraca: modo=$mode, resolução=${resolution}x${resolution}, threads=$threads ($RUNS repetições)..."
            case_results="$TEMP_DIR/${mode}_${resolution}.csv"
            THREADS="$threads" SCHEDULES="$WEAK_SCHEDULES" CHUNKS="$WEAK_CHUNKS" RUNS="$RUNS" \
                BENCHMARK_TYPE=strong MODE_NAME="$mode" \
                MODE_ARGS="${mode_flag:+$mode_flag }--width $resolution --height $resolution" \
                RESULTS_FILE="$case_results" ENVIRONMENT_FILE=/dev/null \
                "$ROOT_DIR/scripts/benchmark.sh" >/dev/null
            awk -F, -v resolution="$resolution" 'NR > 1 { print resolution "," resolution "," $0 }' \
                "$case_results" >> "$RESULTS_FILE"
        done
    done

    echo "Resultados gravados em $RESULTS_FILE"
    echo "Ambiente registrado em $ENVIRONMENT_FILE"
    exit 0
fi

if [ "$BENCHMARK_TYPE" != "strong" ]; then
    echo "ERRO: BENCHMARK_TYPE deve ser strong ou weak." >&2
    exit 1
fi

THREADS=${THREADS:-"$(seq 2 "$MAX_THREADS" | tr '\n' ' ')"}
SCHEDULES=${SCHEDULES:-"static dynamic guided"}
CHUNKS=${CHUNKS:-"1 16 64"}
RUNS=${RUNS:-3}
MODE_ARGS=${MODE_ARGS:-""}
if [ -z "${MODE_NAME:-}" ]; then
    case " $MODE_ARGS " in
        *" --seahorse "*) MODE_NAME=seahorse ;;
        *) MODE_NAME=padrao ;;
    esac
fi
RESULTS_FILE=${RESULTS_FILE:-"$ROOT_DIR/results/benchmark_${MODE_NAME}.csv"}
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

for threads in $THREADS; do
    case "$threads" in
        *[!0-9]*|'')
            echo "ERRO: THREADS deve conter apenas inteiros positivos." >&2
            exit 1
            ;;
    esac
    if [ "$threads" -lt 2 ] || [ "$threads" -gt "$MAX_THREADS" ]; then
        echo "ERRO: cada valor de THREADS deve estar entre 2 e $MAX_THREADS." >&2
        exit 1
    fi
done

make -C "$ROOT_DIR" -f Makefile.seq all >/dev/null
make -C "$ROOT_DIR" -f Makefile.openmp all >/dev/null
mkdir -p "$(dirname -- "$RESULTS_FILE")"
{
    echo ""
    echo "=== Benchmark: $MODE_NAME ==="
    echo "Data: $(date -Is)"
    echo "Sistema: $(uname -a)"
    echo "Compilador: $(gcc --version | sed -n '1p')"
    if command -v lscpu >/dev/null 2>&1; then
        lscpu
    fi
} >> "$ENVIRONMENT_FILE"
printf 'modo,repeticao,threads,escalonamento,chunk,t1_s,tp_s,speedup,eficiencia,fator_balanceamento\n' > "$RESULTS_FILE"

for repetition in $(seq 1 "$RUNS"); do
    (
        cd "$TEMP_DIR"
        # A expansão intencional de MODE_ARGS permite passar opções como --seahorse.
        "$ROOT_DIR/mandelbrot_seq" $MODE_ARGS > sequencial.log
        t1=$(awk '/Tempo de calculo \(T1\)/ { print $(NF - 1) }' sequencial.log)

        for threads in $THREADS; do
            for schedule in $SCHEDULES; do
                for chunk in $CHUNKS; do
                    "$ROOT_DIR/mandelbrot_openmp" "$threads" $MODE_ARGS \
                        --schedule "$schedule" --chunk "$chunk" > openmp.log
                    tp=$(awk '/Tempo de calculo \(Tp\)/ { print $(NF - 1) }' openmp.log)
                    lbf=$(awk '/Fator de Balanceamento/ { print $(NF - 1) }' openmp.log)
                    speedup=$(awk -v t1="$t1" -v tp="$tp" 'BEGIN { printf "%.9f", t1 / tp }')
                    efficiency=$(awk -v speedup="$speedup" -v threads="$threads" 'BEGIN { printf "%.9f", speedup / threads }')
                    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
                        "$MODE_NAME" "$repetition" "$threads" "$schedule" "$chunk" \
                        "$t1" "$tp" "$speedup" "$efficiency" "$lbf" >> "$RESULTS_FILE"
                done
            done
        done
    )
done

echo "Resultados gravados em $RESULTS_FILE"
echo "Ambiente registrado em $ENVIRONMENT_FILE"
