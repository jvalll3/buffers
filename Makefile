#include Makefile.defs
CC = gcc
MPICC = mpicc
CXX = g++
CFLAGS  = -O2
LDFLAGS =
GFLAGS = -Og -g

CFLAGS_PAR = $(CFLAGS) `sionconfig --cflags --ompi --c`
LDFLAGS_PAR = $(LDFLAGS) `sionconfig --libs --ompi --c`
CFLAGS_PAR2 = $(CFLAGS) `sionconfig --cflags --mpi --c`
LDFLAGS_PAR2 = $(LDFLAGS) `sionconfig --libs --mpi --c`
CFLAGS_PAR3 = $(CFLAGS) `sionconfig --cflags --omp --c`
LDFLAGS_PAR3 = $(LDFLAGS) `sionconfig --libs --omp --c`

TARGET  = buffer mpi mpiomp omp
OBJECTS = $(TARGET)

all: $(TARGET)

buffer: buffer.c
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS)

mpi: mpi.c
	$(MPICC) $(CFLAGS_PAR2) -fopenmp mpi.c -o mpi $(LDFLAGS_PAR2)

mpiomp: mpiomp.c
	$(MPICC) $(CFLAGS_PAR) -fopenmp mpiomp.c -o mpiomp $(LDFLAGS_PAR)

omp: omp.c
	$(CC) $(CFLAGS_PAR3) -fopenmp omp.c -o omp $(LDFLAGS_PAR3)

clean: out_clean
	rm -rf $(OBJECTS) res_* buffer_* *.sion mpi_* mpiomp_* omp_* res/*

out_clean:


submit: buffer
