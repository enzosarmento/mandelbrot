#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
THREADS=${THREADS:-4}
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT HUP INT TERM

make -C "$ROOT_DIR" -f Makefile.seq all >/dev/null
make -C "$ROOT_DIR" -f Makefile.openmp all >/dev/null

(
    cd "$TEMP_DIR"
    "$ROOT_DIR/mandelbrot_seq" "$@" >/dev/null
    "$ROOT_DIR/mandelbrot_openmp" "$THREADS" "$@" >/dev/null

    if ! cmp -s mandelbrot.bin mandelbrot_openmp.bin; then
        echo "ERRO: as matrizes binárias diferem." >&2
        exit 1
    fi

    echo "Corretude aprovada: os arquivos .bin são idênticos."
    sha256sum mandelbrot.bin mandelbrot_openmp.bin
)
