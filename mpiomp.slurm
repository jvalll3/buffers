#!/bin/bash
#SBATCH --job-name=mpiomp1
#SBATCH --output=mpiomp1_%j.out
#SBATCH --error=mpiomp1_%j.err
#SBATCH --ntasks=2
#SBATCH --cpus-per-task=24
#SBATCH --tasks-per-node=2
#SBATCH --time=00:40:00
#SBATCH --exclusive
module unload intel
module load gcc

export OMP_NUM_THREADS=24

srun ./mpiomp 40000000 10485760 1 256
