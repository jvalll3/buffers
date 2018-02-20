#!/bin/bash
#SBATCH --job-name=mpi64i
#SBATCH --ntasks=3072
#SBATCH --time=00:30:00
#SBATCH --output=mpi64i_%j.out
#SBATCH --error=mpi64i_%j.err
#SBATCH --exclusive

module load gcc/5.4.0

srun ./mpi 40000000 10485760 1 256

#######SBATCH --cpus-per-task=48
#######SBATCH --tasks-per-node=1
