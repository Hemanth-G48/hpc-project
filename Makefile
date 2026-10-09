CC       ?= gcc
CFLAGS   := -O3 -g -fno-omit-frame-pointer -std=gnu11 -Wall -Wextra
OMPFLAGS := -fopenmp
INC      := -I.

SEQ_BINS := build/matmul_seq build/heat2d_seq build/kmeans_seq \
            build/map_seq build/reduction_seq build/sparselu_seq build/vecmul_seq
OMP_BINS := build/matmul_omp build/heat2d_omp build/kmeans_omp \
            build/map_omp build/reduction_omp build/sparselu_omp build/vecmul_omp
PS_BINS  := build/matmul_ps build/map_ps build/reduction_ps build/vecmul_ps
TASK_BINS := build/map_tasks build/vecmul_tasks build/reduction_tasks build/matmul_tasks

.PHONY: all seq omp ps tasks clean

all: $(SEQ_BINS) $(OMP_BINS) $(PS_BINS) $(TASK_BINS)

seq: $(SEQ_BINS)

omp: $(OMP_BINS)

ps: $(PS_BINS)

tasks: $(TASK_BINS)

build:
	mkdir -p build

build/%_seq: sequential/%_seq.c common.h | build
	$(CC) $(CFLAGS) $(INC) $< -o $@ -lm

build/%_omp: openmp/%_omp.c common.h | build
	$(CC) $(CFLAGS) $(OMPFLAGS) $(INC) $< -o $@ -lm

build/%_ps: paper_strategy/%_ps.c paper_strategy/ps_runtime.c paper_strategy/ps_runtime.h common.h | build
	$(CC) $(CFLAGS) $(INC) -Ipaper_strategy $< paper_strategy/ps_runtime.c -o $@ -lm -pthread

build/%_tasks: paper_strategy_openmp/%_tasks.c paper_strategy_openmp/omp_paper.h common.h | build
	$(CC) $(CFLAGS) $(OMPFLAGS) $(INC) -Ipaper_strategy_openmp $< -o $@ -lm

clean:
	rm -rf build
