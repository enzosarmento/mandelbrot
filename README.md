# Conjunto de Mandelbrot com OpenMP e MPI

Implementação em C para calcular e visualizar o conjunto de Mandelbrot. O projeto possui versões sequencial, paralela em memória compartilhada com OpenMP e paralela distribuída com MPI, além de um modo de aproximação para o Vale dos Cavalos-Marinhos.

## Estrutura do projeto

```
.
├── src/        # implementações C: sequencial, OpenMP, MPI e núcleo comum
├── include/    # cabeçalhos públicos
├── scripts/    # verificação de corretude e coleta de benchmarks
├── docs/       # apresentação, relatório e notebooks de análise
├── results/    # CSVs, tabelas e gráficos gerados pelos notebooks
├── output/     # imagens e matrizes geradas nas execuções (não versionado)
├── build/      # objetos intermediários (não versionado)
└── bin/        # executáveis gerados (não versionado)
```

Os notebooks ficam em `docs/notebooks/`; execute-os a partir da raiz do repositório para que usem os dados de `results/`.

## Requisitos

- GCC com suporte a OpenMP;
- MPI (MPICH ou OpenMPI);
- `make`;

## Compilação

Compile a versão sequencial:

```bash
make -f Makefile.seq
```

Compile a versão paralela OpenMP:

```bash
make -f Makefile.openmp
```

Compile a versão paralela MPI:

```bash
make -f Makefile.mpi
```

Os executáveis gerados ficam em `bin/`: `mandelbrot_seq`, `mandelbrot_openmp` e `mandelbrot_mpi`.

## Execução

### Versão sequencial

```bash
./bin/mandelbrot_seq
./bin/mandelbrot_seq --seahorse
./bin/mandelbrot_seq --width 1920 --height 1080
```

Ou use os alvos do Makefile, que movem os resultados para `output/`:

```bash
make -f Makefile.seq run
make -f Makefile.seq run-seahorse ARGS="--width 1920 --height 1080"
```

### Versão OpenMP

O primeiro argumento sem opção define a quantidade de threads. Também é possível usar `--threads`.

```bash
./bin/mandelbrot_openmp 8
./bin/mandelbrot_openmp 8 --seahorse
./bin/mandelbrot_openmp --threads 8 --width 1920 --height 1080 --schedule guided --chunk 16
```

Com Make:

```bash
make -f Makefile.openmp run T=8
make -f Makefile.openmp run-seahorse T=8 ARGS="--width 1920 --height 1080"
```

Opções aceitas na versão OpenMP:

- `-s`, `--seahorse`: aproxima a região do Vale dos Cavalos-Marinhos e usa 5000 iterações;
- `-t N`, `--threads N`: define o número de threads (OpenMP);
- `-w N`, `--width N`: largura da imagem;
- `-h N`, `--height N`: altura da imagem;
- `--schedule POLITICA`: escolhe `static`, `dynamic` ou `guided` (OpenMP);
- `--chunk N`: define o tamanho de chunk da política de escalonamento.
- `--no-symmetry`: calcula todas as linhas, desabilitando a simetria usada na vista padrão; use para comparar diretamente com MPI.

### Versão MPI

A versão MPI utiliza as diretrizes e boas práticas abordadas nas aulas de processamento distribuído (Aulas 10 e 11) e no material de *Scatter e Gather com o Filtro Gaussiano*:

- **Entrada e difusão (E/S)**: O processo de rank 0 recebe e valida os parâmetros de linha de comando e os difunde para todos os processos via `MPI_Bcast`.
- **Sem otimização de simetria**: Conforme especificado, todas as linhas são computadas diretamente, garantindo consistência em qualquer partição de domínio.
- **Decomposição em faixas de linhas (Horizontal Slices)**:
  - Se a altura $N$ for divisível pelo número de processos $P$, utiliza-se `MPI_Gather` com blocos contíguos de tamanho igual $N/P$;
  - Se $N$ não for perfeitamente divisível por $P$, utiliza-se `MPI_Gatherv` com vetores de contagens (`recvcounts`) e deslocamentos (`displs`), distribuindo o resto entre os processos iniciais.
- **Distribuição cíclica (opcional)**: Suporte à distribuição *round-robin* de linhas (`--cyclic`), demonstrando como mitigar o desbalanceamento inerente às faixas contíguas em fractais.
- **Métricas e reduções globais**: Usa `MPI_Reduce` (`MPI_MAX`, `MPI_MIN`, `MPI_SUM`) e `MPI_Gather` para aferir e exibir $T_{max}$, $T_{min}$, $T_{avg}$, Fator de Balanceamento ($FLB = (T_{max}-T_{min})/T_{max}$) e detalhes individuais de cada processo.

Execução direta com `mpirun` / `mpiexec`:

```bash
mpirun -np 4 ./bin/mandelbrot_mpi
mpirun -np 4 ./bin/mandelbrot_mpi --seahorse
mpirun -np 4 ./bin/mandelbrot_mpi --width 1920 --height 1080
mpirun -np 4 ./bin/mandelbrot_mpi --cyclic
```

Com Make:

```bash
make -f Makefile.mpi run NP=4
make -f Makefile.mpi run-seahorse NP=4 ARGS="--width 1920 --height 1080"
```

Opções aceitas na versão MPI:

- `-s`, `--seahorse`: aproxima a região do Vale dos Cavalos-Marinhos (5000 iterações);
- `-w N`, `--width N`: largura da imagem;
- `-h N`, `--height N`: altura da imagem;
- `-b`, `--block`: distribuição padrão em faixas de linhas contíguas (`MPI_Gather` / `MPI_Gatherv`);
- `-c`, `--cyclic`: distribuição cíclica (round-robin) para balanceamento de carga;
- `--help`: exibe o resumo de opções.

## Corretude e benchmarks

### Verificação de corretude

Compare automaticamente as matrizes sequencial e OpenMP:

```bash
make -f Makefile.seq check T=4 ARGS="--width 4096 --height 4096"
make -f Makefile.seq check T=4 ARGS="--seahorse --width 4096 --height 4096"
```

Valide a corretude da versão MPI (compara 1 processo vs $NP$ processos e contra a referência sequencial):

```bash
make -f Makefile.mpi check NP=4 ARGS="--width 4096 --height 4096"
make -f Makefile.mpi check NP=4 ARGS="--seahorse --width 4096 --height 4096"
```

O comando falha se os arquivos `.bin` não forem idênticos e imprime seus hashes SHA-256.

Para gerar uma tabela persistente, pronta para citar no relatório, para os casos padrão e Seahorse:

```bash
NP=4 ./scripts/collect_mpi_correctness.sh
```

O arquivo `results/corretude_mpi.csv` registra igualdade exata, número e percentual de pixels divergentes, diferença máxima de iteração e hashes SHA-256.

### Benchmarks OpenMP

Para coletar tempos, Speedup, Eficiência e Fator de Balanceamento para as políticas e chunks OpenMP:

```bash
RUNS=5 ./scripts/benchmark.sh
MODE_ARGS="--seahorse" RUNS=5 ./scripts/benchmark.sh
```

Os resultados padrão são salvos em `results/benchmark_padrao.csv`; os do Seahorse, em `results/benchmark_seahorse.csv`.

### Benchmarks MPI

Para coletar métricas de escalabilidade forte e balanceamento de carga comparando as estratégias de faixas contíguas e cíclica no MPI:

```bash
RUNS=5 PROCESSES="1 2 4 8" ./scripts/benchmark_mpi.sh
MODE_ARGS="--seahorse" RUNS=5 PROCESSES="1 2 4 8" ./scripts/benchmark_mpi.sh
```

Os resultados são gravados em `results/benchmark_mpi_padrao.csv` e `results/benchmark_mpi_seahorse.csv`.

Além das métricas agregadas, cada execução grava `results/benchmark_mpi_<modo>_por_processo.csv`, com o tempo de computação e a quantidade de pixels de cada rank. Isso permite apresentar o FBC junto aos tempos por processo, especialmente no caso Seahorse.

### Escalabilidade fraca MPI

```bash
RUNS=3 ./scripts/benchmark_mpi_weak.sh
MODE_ARGS="--seahorse" RUNS=3 ./scripts/benchmark_mpi_weak.sh
```

O padrão usa os casos `4096x4096:1`, `8192x8192:4` e `16384x16384:16`; altere `WEAK_CASES` caso o hardware não tenha memória ou processos suficientes. Os dados ficam em `results/benchmark_mpi_escalabilidade_fraca.csv`, e `eficiencia_fraca` é definida como `T_base(4096², P=1) / Tp`.

As métricas seguem a Aula 03: `S(n,p) = T(n) / T(n,p)`, `E(n,p) = S(n,p) / p` e
`FLB = (T_max - T_min) / T_max` (0 é o balanceamento ideal). `T(n,p)` é cronometrado
entre barreiras MPI, do início comum ao término do último processo; a E/S dos arquivos
não entra nessa medida. Após esta atualização, gere novamente os CSVs MPI e OpenMP,
pois valores antigos de `fator_balanceamento` usavam outra definição.

Nesta máquina, os 6 núcleos físicos fornecem 12 contextos lógicos por SMT. Portanto,
executar até 12 processos/threads não é *oversubscription* e pode melhorar o tempo de
execução; a eficiência continua sendo calculada com `p` igual ao número de processos ou
threads efetivamente usados. A marca em `p=6` nos gráficos é apenas informativa, não um
limite de eficiência nem uma evidência de perda automática de desempenho.

### Comparação OpenMP sem simetria

Para uma comparação justa com MPI no caso padrão, gere a referência serial e a versão OpenMP sem explorar a simetria:

```bash
RUNS=5 THREADS="1 2 3 4 5 6 7 8 9 10 11 12" \
SCHEDULES=dynamic CHUNKS=1 ./scripts/benchmark_openmp_no_symmetry.sh
```

Os dados são salvos em `results/benchmark_openmp_sem_simetria_padrao.csv`.

## Arquivos gerados

Cada execução salva dois arquivos:

- `.bin`: matriz crua de contagens de iteração em `int32_t`;
- `.ppm`: imagem colorida, no formato Portable Pixmap, indicada para visualização.

## Limpeza

```bash
make -f Makefile.seq clean
make -f Makefile.openmp clean
make -f Makefile.mpi clean
```

## Transparência sobre uso de IA

Declaro que este projeto contou com o auxílio das ferramentas de IA `Codex e Antigravity CLI` exclusivamente para `otimizações no código, para criações de scripts de benchmark e de corretude, padrão de cores para as imagens ppm, criação do readme`. Revisei, testei e validei criticamente todo conteúdo gerado, assumindo responsabilidade exclusiva pela correção lógica do código, pelos resultados de desempenho e pela integridade acadêmica.

`Enzo Fernandes Sarmento, Jéssia Meira Costa` — `18/09/2026`
