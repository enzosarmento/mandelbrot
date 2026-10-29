#!/usr/bin/env sh
# Gera dados OpenMP comparáveis ao MPI: ambos calculam todas as linhas.
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXTRA_ARGS=${MODE_ARGS:-""}

case " $EXTRA_ARGS " in
    *" --seahorse "*) MODE_NAME=${MODE_NAME:-seahorse_sem_simetria} ;;
    *) MODE_NAME=${MODE_NAME:-padrao_sem_simetria} ;;
esac

RESULTS_FILE=${RESULTS_FILE:-"$ROOT_DIR/results/benchmark_openmp_sem_simetria_${MODE_NAME%%_sem_simetria}.csv"}

MODE_NAME="$MODE_NAME" RESULTS_FILE="$RESULTS_FILE" \
MODE_ARGS="--no-symmetry $EXTRA_ARGS" \
"$ROOT_DIR/scripts/benchmark.sh"
