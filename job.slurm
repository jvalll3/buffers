#!/bin/bash
#SBATCH --job-name=mpi
#SBATCH --output=mpi_%j.out
#SBATCH --error=mpi_%j.err
#SBATCH --ntasks=2
#SBATCH --cpus-per-task=4
#SBATCH --tasks-per-node=1
#SBATCH --time=00:20:00

module load gcc/5.4.0


export OMP_NUM_THREADS=4


#srun ./mpi 40000000 10485760 1
mpirun -n 2 ./mpi 10 380 1
