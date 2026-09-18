# Conjunto de Mandelbrot com OpenMP

Implementação em C para calcular e visualizar o conjunto de Mandelbrot. O projeto possui uma versão sequencial e outra paralela com OpenMP, além de um modo de aproximação para o Vale dos Cavalos-Marinhos.

## Requisitos

- GCC com suporte a OpenMP;
- `make`;

## Compilação

Compile a versão sequencial:

```bash
make -f Makefile.seq
```

Compile a versão paralela:

```bash
make -f Makefile.openmp
```

Os executaveis gerados são `mandelbrot_seq` e `mandelbrot_openmp`.

## Execução

### Versão sequencial

```bash
./mandelbrot_seq
./mandelbrot_seq --seahorse
./mandelbrot_seq --width 1920 --height 1080
```

Ou use os alvos do Makefile, que movem os resultados para `output/`:

```bash
make -f Makefile.seq run
make -f Makefile.seq run-seahorse ARGS="--width 1920 --height 1080"
```

### Versão OpenMP

O primeiro argumento sem opcao define a quantidade de threads. Tambem e possivel usar `--threads`.

```bash
./mandelbrot_openmp 8
./mandelbrot_openmp 8 --seahorse
./mandelbrot_openmp --threads 8 --width 1920 --height 1080 --schedule guided --chunk 16
```

Com Make:

```bash
make -f Makefile.openmp run T=8
make -f Makefile.openmp run-seahorse T=8 ARGS="--width 1920 --height 1080"
```

Opções aceitas:

- `-s`, `--seahorse`: aproxima a região do Vale dos Cavalos-Marinhos e usa 5000 iterações;
- `-t N`, `--threads N`: define o número de threads (OpenMP);
- `-w N`, `--width N`: largura da imagem;
- `-h N`, `--height N`: altura da imagem;
- `--schedule POLITICA`: escolhe `static`, `dynamic` ou `guided` (OpenMP);
- `--chunk N`: define o tamanho de chunk da política de escalonamento.

A versão OpenMP sempre mostra as estatísticas detalhadas de cada thread.

## Corretude e benchmarks

Compare automaticamente as matrizes sequencial e paralela com os mesmos parâmetros:

```bash
make -f Makefile.seq check T=4 ARGS="--width 4096 --height 4096"
make -f Makefile.seq check T=4 ARGS="--seahorse --width 4096 --height 4096"
```

O comando falha se os arquivos `.bin` não forem idênticos e imprime seus hashes SHA-256.

Para coletar tempos, Speedup, Eficiência e Fator de Balanceamento para as políticas e chunks avaliados, execute:

```bash
RUNS=5 ./scripts/benchmark.sh
MODE_ARGS="--seahorse" RUNS=5 ./scripts/benchmark.sh
```

Os resultados padrão são salvos em `results/benchmark_padrao.csv`; os do Seahorse, em `results/benchmark_seahorse.csv`. O ambiente (sistema, compilador e CPU) é registrado em `results/environment.txt`.

Por padrão, o benchmark executa todas as contagens de threads de 2 até o número de CPUs lógicas disponíveis. É possível restringir os testes, mantendo esses limites, por exemplo: `THREADS="2 4 8" RUNS=5 ./scripts/benchmark.sh`. Ajuste também `SCHEDULES`, `CHUNKS`, `RUNS` e `MODE_ARGS` conforme o experimento. Para a análise de escalabilidade fraca, execute o script em resoluções crescentes, por exemplo com `MODE_ARGS="--width 8192 --height 8192"`.

## Arquivos gerados

Cada execução salva dois arquivos:

- `.bin`: matriz crua de contagens de iteração em `int32_t`;
- `.ppm`: imagem colorida, no formato Portable Pixmap, indicada para visualização.

## Limpeza

```bash
make -f Makefile.seq clean
make -f Makefile.openmp clean
```

## Transparência sobre uso de IA

Declaro que este projeto contou com o auxílio das ferramentas de IA `Codex e Antigravity CLI` exclusivamente para `otimizações no código, para criações de scripts de benchmark e de corretude, padrão de cores para as imagens ppm, criação do readme`. Revisei, testei e validei criticamente todo conteúdo gerado, assumindo responsabilidade exclusiva pela correção lógica do código, pelos resultados de desempenho e pela integridade acadêmica.

`Enzo Fernandes Sarmento, Jéssia Meira Costa` — `18/09/2026`
