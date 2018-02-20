#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>


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

void inBuffer2 ( char** position, const char* in, int inLength, int* freeSpace)
{
	*freeSpace -= inLength;
	void * resultMemcopy = memcpy(*position, in, inLength);
	*position += inLength;
}


int partialInBuffer(char** position, char* in, int inLength, int* freeSpace)
{
	void * resultMemcopy = memcpy(*position, in, *freeSpace);
	int copied = *freeSpace;
	*freeSpace -= inLength;
	return copied;
}

void flushBuffer(char* buffer, int dimBuffer, char** position, FILE* file, int* freeSpace)
{
	fwrite(buffer, (dimBuffer-*freeSpace), 1, file);
	*freeSpace = dimBuffer;
	*position = buffer;
}


void showTimes(struct timeval t1, struct timeval t2, int numWrites, int n, char* tag){
	float t_total = (float)((t2.tv_sec - t1.tv_sec)*1000000L)+(t2.tv_usec - t1.tv_usec);
	printf("%s: \t\t %.4f s \t %.4f us/ev \t Writes=%d\n",tag, t_total/1000000,t_total/n, numWrites);
}

void binaryMode (struct event *events, int n, int disableBuffer) {
	//MODE 1: BINARI WITHOUT BUFFER
	struct timeval t1, t2;
	int numWrites = 0;
	FILE *file;
	file = fopen("res_binary", "wb");
	if(disableBuffer == 1){
			setbuf(file, NULL);
	}
	gettimeofday(&t1,NULL);
	fwrite(events, sizeof(struct event), n, file);
	numWrites++;
	gettimeofday(&t2,NULL);
	fclose(file);

	showTimes(t1, t2, numWrites, n, "Binary");
}

void StringMode (struct event *events, int n, int disableBuffer) {
	//MODE 2: STRING WITHOUT BUFFER
	struct timeval t1, t2;
	int numWrites = 0;
	FILE *file;
	file = fopen("res_string", "wt");
	if(disableBuffer == 1){
			setbuf(file, NULL);
	}
	gettimeofday(&t1,NULL);
	for(int i=0;i<n;i++)
	{
		fprintf(file, "%d:%d:%llu\n", events[i].category, events[i].type, events[i].value);
		numWrites++;
	}
	gettimeofday(&t2,NULL);
	fclose(file);
	showTimes(t1, t2, numWrites, n, "Serialization");
}

void StringWithBufferMode (struct event *events, int n, int dimBuffer, int disableBuffer) {
	//MODE 3: STRING WITH BUFFER
	struct timeval t1, t2;
	int numWrites = 0;
	FILE *file;
	file = fopen("res_string_buffer", "wt");
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

	for(int i=0;i<n;i++)
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
	if(freeSpace < dimBuffer)
	{
		flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
		numWrites++;
	}
	gettimeofday(&t2,NULL);
	fclose(file);
	free(buffer);
	showTimes(t1, t2, numWrites, n, "Buffer");
}

void StringWithFullBufferMode (struct event *events, int n, int dimBuffer, int disableBuffer) {
	//MODE 4: STRING WITH BUFFER
	struct timeval t1, t2;
	int numWrites = 0;
	FILE *file;
	file = fopen("res_string_buffer2", "wt");
	if(disableBuffer == 1){
			setbuf(file, NULL);
	}

	char *buffer = (char *)malloc(sizeof(char) * dimBuffer);
	int freeSpace = dimBuffer;
	char *bufferPosition = buffer;
	char tmp[38];
	setbuf(file, NULL);
	gettimeofday(&t1,NULL);

	for(int i=0;i<n;i++)
	{
		int l = sprintf(tmp, "%d:%d:%lli\n", events[i].category, events[i].type,
						events[i].value);
		if(l <= freeSpace){
			//cabe
			inBuffer(&bufferPosition, tmp, l, &freeSpace);
		} else {
			//flush
			int copied =  partialInBuffer(&bufferPosition, tmp, l, &freeSpace);
			freeSpace = 0;
			flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
			numWrites++;
			inBuffer(&bufferPosition, tmp + copied, (l-copied), &freeSpace);
		}
	}
	//flush restante
	if(freeSpace < dimBuffer)
	{
		flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
		numWrites++;
	}

	gettimeofday(&t2,NULL);
	fclose(file);
	free(buffer);
	showTimes(t1, t2, numWrites, n, "Serialization with Buffer (FULL)");
}


void writevMode(struct event *events, int n, int divisions, int disableBuffer){
	struct iovec iov[UIO_MAXIOV];
	struct timeval t1, t2;
	ssize_t nwritten;
	struct event *position = events;
	int l;
	int numWrites = 0;

	int fd1 = open("res_writev", O_WRONLY|O_CREAT|O_TRUNC, 0700);
	int iovcnt = 0;
	gettimeofday(&t1, NULL);

	for (int i=0; i < n; i=i+divisions){
		if(iovcnt < UIO_MAXIOV){
			iov[(iovcnt)].iov_base = position;
			iov[(iovcnt)].iov_len = sizeof(struct event) * divisions;
			iovcnt++;
		} else {
			//printf("iovcnt = %d\n", iovcnt);
			nwritten = writev(fd1, iov, iovcnt);
			numWrites++;
			iov[0].iov_base = position;
			iov[0].iov_len = sizeof(struct event) * divisions;
			iovcnt = 1;
		}
		position += divisions;
	}

	if (iovcnt > 0){

		//printf("final iovcnt = %d\n", iovcnt);
		nwritten = writev(fd1, iov, iovcnt);
		numWrites++;
	}

	/*for (int i=0; i<(n/divisions); i++){

		iov[i].iov_base = position;
		iov[i].iov_len = sizeof(struct event) * divisions;
		position += divisions;

	}
	int iovcnt = (sizeof(iov) / sizeof(struct iovec));
	nwritten = writev(fd1, iov, iovcnt);
	printf("%d\n", UIO_MAXIOV);*/

	gettimeofday(&t2, NULL);

	showTimes(t1, t2, 0, n, "Writev");
	close(fd1);
}

void cpp(struct event *events, int n, int dimBuffer, int disableBuffer){
	struct timeval t1, t2;
	std::stringstream ss;
	FILE *file;
	file = fopen("res_stringstream", "wt");
	char *buffer = (char *)malloc(sizeof(char) * dimBuffer);
	int freeSpace = dimBuffer;
	char *bufferPosition = buffer;

	gettimeofday(&t1,NULL);

	for(int i=0;i<n;i++){
		ss.str(std::string());
		ss << events[i].category << ':' << events[i].type << ':' << events[i].value << std::endl;
		//s.append("\n");
		std::string str = ss.str();
		const char *tmp = str.c_str();
		//char *tmp = &str[0u];
		int l = strlen(tmp);

		if(l <= freeSpace){
			//cabe
			inBuffer2(&bufferPosition, tmp, l, &freeSpace);
		} else {
			//flush
			flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
			inBuffer2(&bufferPosition, tmp, l, &freeSpace);
		}
	}

	//flush restante
	if(freeSpace < dimBuffer)
	{
		flushBuffer(buffer, dimBuffer, &bufferPosition, file, &freeSpace);
	}

	gettimeofday(&t2,NULL);
	free(buffer);
	fclose(file);
	showTimes(t1, t2, 0, n, "Stringstreams");
}


int main(int argc, char *argv[])
{
	int n = atoi(argv[1]);
	int dimBuffer = atoi(argv[2]);
	int divisions = atoi(argv[3]);
	int disableBuffer = 0;
	if(argv[4]){
		disableBuffer = atoi(argv[4]);
	}
	int numWrites = 0;
	if(n < 1)
	{
		printf("ERROR");
		return 1;
	}
	if(n < divisions)
	{
		printf("ERROR divisions > events");
		return 1;
	}

	//Initialize the struct array
	struct event *events = (event*)malloc(sizeof(struct event)*n);
	struct timeval t1, t2;
	unsigned long elapsedTime;

	srand(100);
	//put some random values
	for(int i=0;i<n;i++)
	{
		events[i].category = (short)rand();
		events[i].type = (int) rand();
		events[i].value = (long long) rand();
	}

	binaryMode(events, n, disableBuffer);
	StringMode(events, n, disableBuffer);
	StringWithBufferMode(events, n, dimBuffer, disableBuffer);
	writevMode(events, n, divisions, disableBuffer);
	//StringWithFullBufferMode(events, n, dimBuffer, disableBuffer);
	cpp(events, n, dimBuffer, disableBuffer);
	free(events);

	return 0;
}
