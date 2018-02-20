#!/bin/bash
###declare -a arr=("mpiomp" "mpi" "omp")
###for mode in "${arr[@]}"
###do
for i in 1 2 4 8 16 32
do
ntasks=$(( $i * 2 ))
timeout=
echo "#!/bin/bash" > job_mpiomp_$i.slurm
echo "#SBATCH --job-name=mpiomp_"$i"" >> job_mpiomp_$i.slurm
echo "#SBATCH --output=mpiomp_"$i"_%j.out" >> job_mpiomp_$i.slurm
echo "#SBATCH --error=mpiomp_"$i"_%j.err" >> job_mpiomp_$i.slurm
echo "#SBATCH --ntasks="$ntasks"" >> job_mpiomp_$i.slurm
echo "#SBATCH --cpus-per-task=24" >> job_mpiomp_$i.slurm
echo "#SBATCH --tasks-per-node=2" >> job_mpiomp_$i.slurm

case $i in

1) echo "#SBATCH --time=00:20:00" >> job_mpiomp_$i.slurm
;;

2) echo "#SBATCH --time=01:00:00" >> job_mpiomp_$i.slurm
;;

4) echo "#SBATCH --time=02:00:00" >> job_mpiomp_$i.slurm
;;

8) echo "#SBATCH --time=06:00:00" >> job_mpiomp_$i.slurm
;;

16) echo "#SBATCH --time=12:00:00" >> job_mpiomp_$i.slurm
;;

32) echo "#SBATCH --time=20:00:00" >> job_mpiomp_$i.slurm
;;


esac

echo "#SBATCH --exclusive" >> job_mpiomp_$i.slurm
echo "module unload intel" >> job_mpiomp_$i.slurm
echo "module load gcc" >> job_mpiomp_$i.slurm
echo "export OMP_NUM_THREADS=24" >> job_mpiomp_$i.slurm
echo "srun ./mpiomp 40000000 10485760 1 256 $i" >> job_mpiomp_$i.slurm

sbatch job_mpiomp_"$i".slurm

done
###done
