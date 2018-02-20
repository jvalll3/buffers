#!/bin/bash
#SBATCH --job-name=buffer
#SBATCH --output=buffer_%j.out
#SBATCH --error=buffer_%j.err
#SBATCH --cpus-per-task=4
#SBATCH --tasks-per-node=1
#SBATCH --time=00:20:00

module load gcc/5.4.0

#srun ./buffer 10 300 1
srun ./buffer 40000000 10485760 1 100
