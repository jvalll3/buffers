#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <mpi.h>
#include <math.h>

#include "sion.h"
#define FNAMELEN 255
#define FOLDERNAMESIZE 20
#define TAG_LENGTH 30
#define FILENAME_LENGTH 30
#define EVENT_STRING_LENGTH 38
#define DEFAULT_FOLDERS_SIZE 256
#define HIERARCHY_DIRECTORIES 1

struct event
{
	long long value;
	unsigned int type;
	unsigned short category;
};

void inBuffer ( char** position, char* in, int inLength, int* freeSpace)
{
	*freeSpace -= inLength;
	void * resultMemcopy = memcpy(*position, in, inLength);
	*position += inLength;
}

void flushBuffer(char* buffer, int dimBuffer, char** position, FILE* file, int* freeSpace)
{
	fwrite(buffer, (dimBuffer-*freeSpace), 1, file);
	*freeSpace = dimBuffer;
	*position = buffer;
}

void flushBufferSIONlib(char* buffer, int dimBuffer, char** position, int* freeSpace, int sid, sion_int64 chunksize)
{
	//int rc=sion_ensure_free_space(sid, chunksize);
	//if(rc){
		size_t bwrote = sion_fwrite(buffer, (dimBuffer-*freeSpace), 1, sid);
		//bwrote=fwrite(buffer,1,chunksize,fp);
	//}
	sion_int64 bytes_written = sion_get_bytes_written(sid);
	*freeSpace = dimBuffer;
	*position = buffer;
}

void showTimes(struct timeval t1, struct timeval t2, int numWrites, int n, char* tag){
	float t_total = (float)((t2.tv_sec - t1.tv_sec)*1000000L)+(t2.tv_usec - t1.tv_usec);
	printf("%s \t %.4f s \t %.4f us/ev \t Writes=%d\n",tag, t_total/1000000,t_total/n, numWrites);
}

void allocateBuffers(int nthreads, int nevents, struct event ***events)
{	
	*events = (struct event**)malloc(sizeof(struct event *)*nthreads);
	int i;
	for(i = 0; i < nthreads; i++)
	{
		(*events)[i] = malloc(sizeof(struct event)*nevents);
	}	
}

void fillWithRandomValues(int nevents, struct event ***events)
{
	#pragma omp parallel
	{
		int thread = omp_get_thread_num();
		int z;
		for(z = 0; z < nevents; z++)
		{
			(*events)[thread][z].category = (short)rand();
			(*events)[thread][z].type = (int) rand();
			(*events)[thread][z].value = (long long) rand();
		}
	}
}


void withSionLibBinary(struct event **events, int n, int disableBuffer, int rank, int size, char* nameFolderNodes)
{
	#pragma omp parallel
	{
		struct timeval t1, t2;
		char tmp[EVENT_STRING_LENGTH];
		char nameSionFile[FOLDERNAMESIZE];	
		sprintf(nameSionFile, "%s/mpiompB.sion", nameFolderNodes);

		/* for SIONlib open call */
		int numFiles = 0;
		MPI_Comm lComm;
		sion_int64 chunksize = 0;
		sion_int64 bytes_written=-1;
		sion_int32 fsblksize = 0;
		int globalrank = 0;
		FILE* fp = NULL;
		char* newfname = NULL;
		size_t bwrote;
		int i, rc;
		int numWrites = 0;
		/* other variables */
		int sid = 0;

		/* initial parameters */
		numFiles = 1;
		chunksize = n*sizeof(struct event);
		fsblksize = -1;
		globalrank = rank;
		fp = NULL;

		int thread = omp_get_thread_num();		

		sid = sion_paropen_ompi(nameSionFile, "bw", &numFiles, MPI_COMM_WORLD, &lComm, &chunksize, &fsblksize, &globalrank, &fp, &newfname);
		if(sid>=0)
		{
			gettimeofday(&t1,NULL);
			size_t bwrote = sion_fwrite(events[thread], (sizeof(struct event)*n), 1, sid);
			numWrites++;
			gettimeofday(&t2,NULL);
			
			char tag[TAG_LENGTH];
			sprintf(tag, "With SionLib Binary Rank %d Thread %d", rank, thread);
			showTimes(t1, t2, numWrites, n, tag);
			sion_parclose_ompi(sid);

		} else {
			fprintf(stderr, "on MPI rank %d, OMP thread %d: error sid = %d\n", rank, thread, sid);
		}

		#pragma omp master
		{
			MPI_Barrier(MPI_COMM_WORLD);
		}
					
		#pragma omp barrier

	}

	MPI_Barrier(MPI_COMM_WORLD);
}

void withSionLibSerialization(struct event **events, int n, int dimBuffer, int disableBuffer, int rank, int size, char* nameFolderNodes)
{
	#pragma omp parallel
	{
		struct timeval t1, t2;
		char tmp[EVENT_STRING_LENGTH];	
		char nameSionFile[FOLDERNAMESIZE];	
		sprintf(nameSionFile, "%s/mpiompS.sion", nameFolderNodes);

		/* for SIONlib open call */
		int numFiles = 0;
		MPI_Comm lComm;
		sion_int64 chunksize = 0;
		sion_int64 bytes_written=-1;
		sion_int32 fsblksize = 0;
		int globalrank = 0;
		FILE* fp = NULL;
		char* newfname = NULL;
		size_t bwrote;
		int i, rc;
		int numWrites = 0;
		/* other variables */
		int sid = 0;

		/* initial parameters */
		numFiles = 1;
		chunksize = dimBuffer;
		//chunksize = EVENT_STRING_LENGTH*(n/10);
		fsblksize = -1;
		globalrank = rank;
		fp = NULL;

		int freeSpace = dimBuffer;

		int thread = omp_get_thread_num();
		sid = sion_paropen_ompi(nameSionFile, "bw", &numFiles, MPI_COMM_WORLD, &lComm, &chunksize, &fsblksize, &globalrank, &fp, &newfname);
		if(sid>=0)
		{
			char *buffer = (char *)malloc(sizeof(char)*dimBuffer);
			char *bufferPosition = buffer;
			
			gettimeofday(&t1,NULL);
			
			for(i=0;i<n;i++)
			{
				int l = sprintf(tmp, "%d:%d:%lli\n", events[thread][i].category, events[thread][i].type, events[thread][i].value);
				if(l<=freeSpace){
					inBuffer(&bufferPosition, tmp, l, &freeSpace);
				} else {
					//flush
					flushBufferSIONlib(buffer, dimBuffer, &bufferPosition, &freeSpace, sid, chunksize);
					numWrites++;
					inBuffer(&bufferPosition, tmp, l, &freeSpace);
				}	
			}
			if(freeSpace < dimBuffer)
			{
				flushBufferSIONlib(buffer, dimBuffer, &bufferPosition, &freeSpace, sid, chunksize);
				numWrites++;
			}
			gettimeofday(&t2,NULL);
			char tag[TAG_LENGTH];
			sprintf(tag, "With SionLib Serialization Rank %d Thread %d", rank, thread);
			showTimes(t1, t2, numWrites, n, tag);
			sion_parclose_ompi(sid);
			free(buffer);

		} else {
			fprintf(stderr, "on MPI rank %d, OMP thread %d: error sid = %d\n", rank, thread, sid);
		}

		#pragma omp master
		{
			MPI_Barrier(MPI_COMM_WORLD);
		}
					
		#pragma omp barrier

	}

	MPI_Barrier(MPI_COMM_WORLD);
}

void withoutSionLibBinary(struct event **events, int n, int disableBuffer, int rank, int size, char* nameFolderRank)
{
	#pragma omp parallel
	{
		int thread = omp_get_thread_num();
		struct timeval t1, t2;
		int numWrites = 0;
		FILE *file;
		char nameFile[FILENAME_LENGTH];

		sprintf(nameFile, "%s/resB_ompi_%d_%d", nameFolderRank, rank, thread);
		file = fopen(nameFile, "wt");
		if(disableBuffer == 1){
			setbuf(file, NULL);
		}

		gettimeofday(&t1,NULL);

		fwrite(events[thread], sizeof(struct event), n, file);
		numWrites++;		

		gettimeofday(&t2,NULL);
		fclose(file);
		
		char tag[TAG_LENGTH];
		sprintf(tag, "Without SionLib Binary Rank %d Thread %d", rank, thread);
		showTimes(t1, t2, numWrites, n, tag);

	}
	MPI_Barrier(MPI_COMM_WORLD);	

}


void withoutSionLibSerialization(struct event **events, int n, int dimBuffer, int disableBuffer, int rank, int size, char* nameFolderRank)
{
	
	#pragma omp parallel
	{
		int thread = omp_get_thread_num();
		struct timeval t1, t2;
		int numWrites = 0;
		FILE *file;
		char nameFile[FILENAME_LENGTH];
		sprintf(nameFile, "%s/resS_ompi_%d_%d", nameFolderRank,rank, thread);
		file = fopen(nameFile, "wt");
		if(disableBuffer == 1){
			setbuf(file, NULL);
		}

		char *buffer = (char *)malloc(sizeof(char) * dimBuffer);
		int freeSpace = dimBuffer;
		char *bufferPosition = buffer;
		char tmp[EVENT_STRING_LENGTH];
		if(disableBuffer == 1){
			setbuf(file, NULL);
		}
		gettimeofday(&t1,NULL);

		int i;
		for(i=0;i<n;i++)
		{		
			int l = sprintf(tmp, "%d:%d:%lli\n", events[thread][i].category, events[thread][i].type, events[thread][i].value);
			if(l <= freeSpace){
				//cabe
				inBuffer(&bufferPosition, tmp, l, &freeSpace);
			} else {
				//flush
				flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
				numWrites++;
				inBuffer(&bufferPosition, tmp, l, &freeSpace);
			}
		}

		
		//flush restante
		if(freeSpace < dimBuffer){	
			flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
			numWrites++;
		}

		gettimeofday(&t2,NULL);
		fclose(file);
		free(buffer);
		char tag[TAG_LENGTH];
		sprintf(tag, "Without SionLib Serialization Rank %d Thread %d", rank, thread);
		showTimes(t1, t2, numWrites, n, tag);

	}
	MPI_Barrier(MPI_COMM_WORLD);	

}

int main(int argc, char *argv[])
{
	//GET INITIAL PARAMETERS ---------------------------------------------------
	int n = atoi(argv[1]);
	int dimBuffer = atoi(argv[2]);
	int disableBuffer = 0;
	int foldersSize = DEFAULT_FOLDERS_SIZE;
	int nodes = 1;
	if(argv[3]){
		disableBuffer = atoi(argv[3]);
	}
	if(argv[4]){
		foldersSize = atoi(argv[4]);
	}
	if(argv[5]){
		nodes = atoi(argv[5]);
	}
	int numWrites = 0;
	if(n < 1)
	{
		printf("ERROR");
		return 1;
	}
	
	// GET NUMBER OF THREADS --------------------------------------------------
	int nthreads = 0;
	#pragma omp parallel
	{
		nthreads = omp_get_num_threads();
	}	

 	// Initialize the struct array---------------------------------------------
	struct event **events = NULL;
	allocateBuffers(nthreads, n, &events);
	struct timeval t1, t2;
	unsigned long elapsedTime;
	int rank = 0;
	int size = 0;

	srand(100);
	// Fill events buffer with randomValues
	fillWithRandomValues(n, &events);
	
	MPI_Init (&argc, &argv);
  	MPI_Comm_size(MPI_COMM_WORLD, &size);
  	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Barrier(MPI_COMM_WORLD);	

	int numTasks = size;
	int nfolders = (int)ceil((float)numTasks/foldersSize);
	char folders[nfolders][FOLDERNAMESIZE];
	int myFolderIndex = rank/foldersSize;
	char nameFolderRank[FOLDERNAMESIZE];
	char nameFolderNodes[FOLDERNAMESIZE];
	char results[numTasks][TAG_LENGTH];

	int status;
	sprintf(nameFolderNodes, "res/%d", nodes);
	status = mkdir(nameFolderNodes, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);	

	if(HIERARCHY_DIRECTORIES){
		//Create the folders for the tasks in groups of foldersSize
		if(rank == 0)
		{
			int temp = 0;
			for(int i=0; i < nfolders; i++)
			{
				char nameFolder[FOLDERNAMESIZE];
				sprintf(nameFolder, "%s/%d-%d", nameFolderNodes, temp, (temp + (foldersSize-1)));
				status = mkdir(nameFolder, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
				temp += foldersSize;
				strcpy(folders[i], nameFolder);
			}
		}
		
		//Send the char** with the foldersname to the other ranks
		MPI_Bcast(&folders, nfolders*FOLDERNAMESIZE, MPI_CHAR, 0, MPI_COMM_WORLD);

		//Create the rank folder inside the correct folder
		sprintf(nameFolderRank, "%s/%d", folders[myFolderIndex], rank);
		status = mkdir(nameFolderRank, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);	
	} else {
		sprintf(nameFolderRank, "res");
	}
	
	//Do the tests
	withoutSionLibBinary(events, n, disableBuffer, rank, size, nameFolderRank);
	//withoutSionLibSerialization(events, n, dimBuffer, disableBuffer, rank, size, nameFolderRank);
	withSionLibBinary(events, n, disableBuffer, rank, size, nameFolderNodes);
	//withSionLibSerialization(events, n, dimBuffer, disableBuffer, rank, size, nameFolderNodes);
	
	MPI_Finalize();

	free(events);

  return 0;
}
