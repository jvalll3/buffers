#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#include "sion.h"
#define FNAMELEN 255

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
	//printf("%d = sion_ensure_free_space(%d, %lli)\n", rc, sid, chunksize);
	if(rc){
		size_t bwrote = sion_fwrite(buffer, (dimBuffer-*freeSpace), 1, sid);
		//bwrote=fwrite(buffer,1,chunksize,fp);
	}
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

void withSionLib(struct event **events, int n, int dimBuffer, int disableBuffer)
{
	#pragma omp parallel
	{
		struct timeval t1, t2;
		char tmp[38];	

		/* for SIONlib open call */
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
		chunksize = 38*n;
		fsblksize = -1;
		fp = NULL;

		int freeSpace = dimBuffer;

		int thread = omp_get_thread_num();
		sid = sion_paropen_omp("omp.sion", "bw", &chunksize, &fsblksize, &globalrank, &fp, &newfname);
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
			char tag[30];
			sprintf(tag, "With SionLib Thread %d", thread);
			showTimes(t1, t2, numWrites, n, tag);
			sion_parclose_omp(sid);
			free(buffer);

		} else {
			fprintf(stderr, "on OMP thread %d: error sid = %d\n", thread, sid);
		}

		#pragma omp barrier



	}

}

void withoutSionLib(struct event **events, int n, int dimBuffer, int disableBuffer)
{
	
	#pragma omp parallel
	{
		int thread = omp_get_thread_num();
		struct timeval t1, t2;
		int numWrites = 0;
		FILE *file;
		char nameFile[30];
		sprintf(nameFile, "res_omp_%d", thread);
		file = fopen(nameFile, "wt");
		if(disableBuffer == 1){
			setbuf(file, NULL);
		}

		char *buffer = (char *)malloc(sizeof(char) * dimBuffer);
		int freeSpace = dimBuffer;
		char *bufferPosition = buffer;
		char tmp[38];
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
		char tag[30];
		sprintf(tag, "Without SionLib Thread %d", thread);
		showTimes(t1, t2, numWrites, n, tag);

	}

}

int main(int argc, char *argv[])
{
	//GET INITIAL PARAMETERS ---------------------------------------------------
	int n = atoi(argv[1]);
	int dimBuffer = atoi(argv[2]);
	int disableBuffer = 0;
	if(argv[3]){
		disableBuffer = atoi(argv[3]);
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

	srand(100);
	// Fill events buffer with randomValues
	fillWithRandomValues(n, &events);
	
	withoutSionLib(events, n, dimBuffer, disableBuffer);
	withSionLib(events, n, dimBuffer, disableBuffer);
	
	free(events);

  return 0;
}
