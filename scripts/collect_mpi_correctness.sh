#!/usr/bin/env sh
# Registra o critério de corretude MPI em CSV, para inclusão no relatório.
set -eu

export LC_ALL=C
export MPICH_CC=gcc

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
NP=${NP:-4}
WIDTH=${WIDTH:-4096}
HEIGHT=${HEIGHT:-4096}
CASES=${CASES:-"padrao seahorse"}
RESULTS_FILE=${RESULTS_FILE:-"$ROOT_DIR/results/corretude_mpi.csv"}
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

make -C "$ROOT_DIR" -f Makefile.seq all >/dev/null
make -C "$ROOT_DIR" -f Makefile.mpi all >/dev/null
mkdir -p "$(dirname -- "$RESULTS_FILE")"
printf 'caso,largura,altura,max_iter,processos,estrategia,igualdade_exata,pixels_divergentes,percentual_divergente,max_diferenca_iteracao,sha256_serial,sha256_mpi\n' > "$RESULTS_FILE"

(
    cd "$TEMP_DIR"
    for case_name in $CASES; do
        case "$case_name" in
            padrao)
                max_iter=1000
                serial_args="--no-symmetry --width $WIDTH --height $HEIGHT"
                mpi_args="--width $WIDTH --height $HEIGHT"
                ;;
            seahorse)
                max_iter=5000
                serial_args="--seahorse --width $WIDTH --height $HEIGHT"
                mpi_args="--seahorse --width $WIDTH --height $HEIGHT"
                ;;
            *)
                echo "ERRO: CASES aceita apenas 'padrao' e/ou 'seahorse'." >&2
                exit 1
                ;;
        esac

        # A referência padrão desabilita a simetria para corresponder ao domínio MPI.
        "$ROOT_DIR/bin/mandelbrot_seq" $serial_args >/dev/null
        mv mandelbrot.bin "serial_${case_name}.bin"
        sha_serial=$(sha256sum "serial_${case_name}.bin" | awk '{print $1}')

        for strategy in block cyclic; do
            mpirun -np "$NP" "$ROOT_DIR/bin/mandelbrot_mpi" $mpi_args "--$strategy" >/dev/null
            if ! cmp -s "serial_${case_name}.bin" mandelbrot_mpi.bin; then
                echo "ERRO: $case_name/$strategy divergiu da referência serial." >&2
                exit 1
            fi
            sha_mpi=$(sha256sum mandelbrot_mpi.bin | awk '{print $1}')
            pixels=$((WIDTH * HEIGHT))
            printf '%s,%s,%s,%s,%s,%s,sim,0,0.000000,0,%s,%s\n' \
                "$case_name" "$WIDTH" "$HEIGHT" "$max_iter" "$NP" "$strategy" \
                "$sha_serial" "$sha_mpi" >> "$RESULTS_FILE"
        done
    done
)

echo "Corretude MPI registrada em $RESULTS_FILE"
