#!/usr/bin/env sh
set -eu
export MPICH_CC=gcc

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
NP=${NP:-4}
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

make -C "$ROOT_DIR" -f Makefile.mpi all >/dev/null
make -C "$ROOT_DIR" -f Makefile.seq all >/dev/null

(
    cd "$TEMP_DIR"
    # 1. Executa MPI com 1 processo (referencia)
    mpirun -np 1 "$ROOT_DIR/bin/mandelbrot_mpi" "$@" >/dev/null
    mv mandelbrot_mpi.bin mandelbrot_mpi_np1.bin

    # 2. Executa MPI com NP processos
    mpirun -np "$NP" "$ROOT_DIR/bin/mandelbrot_mpi" "$@" >/dev/null

    if ! cmp -s mandelbrot_mpi_np1.bin mandelbrot_mpi.bin; then
        echo "ERRO: as matrizes binárias diferem entre NP=1 e NP=$NP." >&2
        exit 1
    fi

    echo "Corretude MPI aprovada: matrizes com 1 e $NP processos são idênticas."
    sha256sum mandelbrot_mpi_np1.bin mandelbrot_mpi.bin

    # 3. Compara com a versao sequencial (sem simetria)
    case " $* " in
        *" -s "*|*" --seahorse "*)
            "$ROOT_DIR/bin/mandelbrot_seq" "$@" >/dev/null
            ;;
        *)
            "$ROOT_DIR/bin/mandelbrot_seq" --no-symmetry "$@" >/dev/null
            ;;
    esac

    if ! cmp -s mandelbrot.bin mandelbrot_mpi.bin; then
        echo "ERRO: matriz binária MPI difere da versão sequencial de referência." >&2
        exit 1
    fi

    echo "Corretude comparativa aprovada: matriz MPI idêntica à versão sequencial."
    sha256sum mandelbrot.bin mandelbrot_mpi.bin
)
