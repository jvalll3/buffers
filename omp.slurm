#!/bin/bash
#SBATCH --job-name=omp
#SBATCH --output=omp_%j.out
#SBATCH --error=omp_%j.err
#SBATCH --cpus-per-task=4
#SBATCH --tasks-per-node=1
#SBATCH --time=00:20:00

module load gcc/5.4.0

set OMP_NUM_THREADS=4

srun ./omp 10 300 1
#srun ./omp 40000000 10485760 1
#mpirun -n 2 ./mpiomp 40000000 10485760 1
