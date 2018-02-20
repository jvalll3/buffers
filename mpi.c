#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <mpi.h>

#include "sion.h"
#define FNAMELEN 256
#define FOLDERNAMESIZE 20
#define TAG_LENGTH 30
#define FILENAME_LENGTH 30
#define EVENTSTRINGLENGTH 38
#define DEFAULT_FOLDERS_SIZE 256
#define HIERARCHY_DIRECTORIES 0


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
	int rc=sion_ensure_free_space(sid, chunksize);
//	if(rc){
		size_t bwrote = sion_fwrite(buffer, (dimBuffer-*freeSpace), 1, sid);
		//bwrote=fwrite(buffer,1,chunksize,fp);
		sion_int64 bytes_written = sion_get_bytes_written(sid);
		//printf("%lli bytes written on sid = %d\n", bytes_written, sid);
		*freeSpace = dimBuffer;
		*position = buffer;
	//}
}

void showTimes(struct timeval t1, struct timeval t2, int numWrites, int n, char* tag){
	float t_total = (float)((t2.tv_sec - t1.tv_sec)*1000000L)+(t2.tv_usec - t1.tv_usec);
	printf("%s \t %.4f s \t %.4f us/ev \t Writes=%d\n",tag, t_total/1000000,t_total/n, numWrites);
}

void withSionLibBinary(struct event *events, int n, int disableBuffer, int rank, int size)
{
	struct timeval t1, t2;

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
	
	sid = sion_paropen_mpi("res/mpiB.sion", "bw", &numFiles, MPI_COMM_WORLD, &lComm, &chunksize, &fsblksize, &globalrank, &fp, &newfname);
	if(sid>=0)
	{
		gettimeofday(&t1,NULL);
		size_t bwrote = sion_fwrite(events, sizeof(struct event), n, sid);
		numWrites++;	
		gettimeofday(&t2,NULL);
		char tag[TAG_LENGTH];
		sprintf(tag, "With SionLib Binary Rank %d", rank);
		showTimes(t1, t2, numWrites, n, tag);
		sion_parclose_mpi(sid);

	} else {
		printf("on MPI rank %d: error sid = %d\n", rank, sid);
	}	
		
	MPI_Barrier(MPI_COMM_WORLD);	
}

void withSionLibSerialization(struct event *events, int n, int dimBuffer, int disableBuffer, int rank, int size)
{
	struct timeval t1, t2;
	char tmp[EVENTSTRINGLENGTH];	

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
		fsblksize = -1;
		globalrank = rank;
		fp = NULL;
	
		int freeSpace = dimBuffer;

		sid = sion_paropen_mpi("res/mpiS.sion", "bw", &numFiles, MPI_COMM_WORLD, &lComm, &chunksize, &fsblksize, &globalrank, &fp, &newfname);
		if(sid>=0)
		{
			char *buffer = (char *)malloc(sizeof(char)*dimBuffer);
			char *bufferPosition = buffer;
			
			gettimeofday(&t1,NULL);
			
			for(i=0; i<n; i++)
			{	
				int l = sprintf(tmp, "%d:%d:%lli\n", events[i].category, events[i].type, events[i].value);
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
			free(buffer);
			char tag[TAG_LENGTH];
			sprintf(tag, "With SionLib Serialization Rank %d", rank);
			showTimes(t1, t2, numWrites, n, tag);

		} else {
			printf("on MPI rank %d: error sid = %d\n", rank, sid);
		}

		sion_parclose_mpi(sid);
	
		
	MPI_Barrier(MPI_COMM_WORLD);	
}

void withoutSionLibBinary(struct event *events, int n, int disableBuffer, int rank, int size, char* nameFolderRank)
{
	
	MPI_Barrier(MPI_COMM_WORLD);	
		
	struct timeval t1, t2;
	int numWrites = 0;
	FILE *file;
	char nameFile[FILENAME_LENGTH];
		
	sprintf(nameFile, "%s/resB_mpi_%d", nameFolderRank, rank);
	file = fopen(nameFile, "wb");
	if(disableBuffer == 1){
		setbuf(file, NULL);
	}
	gettimeofday(&t1,NULL);
	fwrite(events, sizeof(struct event), n, file);
	numWrites++;
		
	gettimeofday(&t2,NULL);
	fclose(file);
	char tag[TAG_LENGTH];
	sprintf(tag, "Without SionLib Binary Rank %d", rank);
	showTimes(t1, t2, numWrites, n, tag);

	MPI_Barrier(MPI_COMM_WORLD);	

}

void withoutSionLibSerialization(struct event *events, int n, int dimBuffer, int disableBuffer, int rank, int size, char* nameFolderRank)
{
	
	MPI_Barrier(MPI_COMM_WORLD);	
		
	struct timeval t1, t2;
	int numWrites = 0;
	FILE *file;
	char nameFile[FILENAME_LENGTH];
	sprintf(nameFile, "%s/resS_mpi_%d", nameFolderRank, rank);
	file = fopen(nameFile, "wt");
	if(disableBuffer == 1){
		setbuf(file, NULL);
	}

	char *buffer = (char *)malloc(sizeof(char) * dimBuffer);
	int freeSpace = dimBuffer;
	char *bufferPosition = buffer;
	char tmp[EVENTSTRINGLENGTH];
	if(disableBuffer == 1){
		setbuf(file, NULL);
	}
	gettimeofday(&t1,NULL);

	int i;
	for(i=0;i<n;i++)
	{		
		int l = sprintf(tmp, "%d:%d:%lli\n", events[i].category, events[i].type, events[i].value);
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
	sprintf(tag, "Without SionLib Serialization Rank %d", rank);
	showTimes(t1, t2, numWrites, n, tag);

	MPI_Barrier(MPI_COMM_WORLD);	

}

int main(int argc, char *argv[])
{
	//GET INITIAL PARAMETERS ---------------------------------------------------
	int n = atoi(argv[1]);
	int dimBuffer = atoi(argv[2]);
	int disableBuffer = 0;
	int foldersSize = DEFAULT_FOLDERS_SIZE;
	if(argv[3]){
		disableBuffer = atoi(argv[3]);
	}
	if(argv[4]){
		foldersSize = atoi(argv[4]);
	}
	int numWrites = 0;
	if(n < 1)
	{
		printf("ERROR");
		return 1;
	}
	
 	// Initialize the struct array---------------------------------------------
	struct event *events = (struct event*)malloc(sizeof(struct event)*n);
	struct timeval t1, t2;
	unsigned long elapsedTime;
	int rank = 0;
	int size = 0;
	//char **folders = NULL;

	srand(100);
	//srand(time(NULL));
	//Put some random values
	for(int i=0;i<n;i++)
	{
		events[i].category = (short)rand();
		events[i].type = (int)rand();
		events[i].value = (long long)rand();
	}


	MPI_Init (&argc, &argv);
  	MPI_Comm_size(MPI_COMM_WORLD, &size);
  	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Barrier(MPI_COMM_WORLD);	

	int numTasks = size;
	int nfolders = (int)ceil((float)numTasks/foldersSize);
	char folders[nfolders][FOLDERNAMESIZE];
	int myFolderIndex = rank/foldersSize;
	char nameFolderRank[FOLDERNAMESIZE];
	char results[numTasks][TAG_LENGTH];

	if(HIERARCHY_DIRECTORIES){
		//CREATE THE FOLDERS FOR THE TASKS IN GROUPS OF foldersSize	
		if(rank == 0){
			int temp = 0;

			for(int i=0; i < nfolders; i++)
			{
				char nameFolder[FOLDERNAMESIZE];
				sprintf(nameFolder, "res/%d-%d", temp, (temp + (foldersSize-1)));
				int status = mkdir(nameFolder, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
				temp += foldersSize;
				strcpy(folders[i],nameFolder);
			}
		}
	
		//SEND THE CHAR** WITH THE FOLDERS NAME TO THE OTHER RANKS
		MPI_Bcast(&folders, nfolders*FOLDERNAMESIZE, MPI_CHAR, 0, MPI_COMM_WORLD);	

		//CREATE THE RANK FOLDER INSIDE THE CORRECT FOLDER
		int status;
		sprintf(nameFolderRank, "%s/%d", folders[myFolderIndex], rank);
		status = mkdir(nameFolderRank, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	} else {
		sprintf(nameFolderRank, "res");
	}
	//DO THE TESTS
	withoutSionLibBinary(events, n, disableBuffer, rank, size, nameFolderRank);
	//withoutSionLibSerialization(events, n, dimBuffer, disableBuffer, rank, size, nameFolderRank);
	withSionLibBinary(events, n, disableBuffer, rank, size);
	//withSionLibSerialization(events, n, dimBuffer, disableBuffer, rank, size);
	
	MPI_Finalize();

	free(events);

  return 0;
}
