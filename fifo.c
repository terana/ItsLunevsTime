#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

#define SIZE 100 // Block size
#define NAMELEN 16 // 5 for pid, 10 for time, 1 for \0
#define FIFOSIZE 8000
static char *dispatch_fifo_name = ".dispatch_fifo_name";

void CrashOnError(int err, char* descr) {
	if(err){
		perror(descr);
		exit(1);
	}
}

void CreateIndividualName(char name[NAMELEN]) {
	int err;
	int ind = 0;  
	// SPRINTF REINVENTED
	for(int n = getpid(); n > 0 && ind < 5; n /= 10){
		name[ind++] = (char)(n % 10 + (int) '0');
	}
	
	struct timeval time;
	err = gettimeofday(&time, NULL);
	CrashOnError(err, "Error creating individual name");

	// SPRINFT REINVENTED AGAIN
	for(long n = time.tv_sec; n > 0 && ind < NAMELEN - 1; n /= 10){
		name[ind++] = (char)(n % 10 + (int) '0');
	}

	while(ind < NAMELEN - 1){
		name[ind++] = 'x';
	}

	name[ind] = '\0';
}

void SendName(char name[NAMELEN]) {
	struct stat st;
	int err;
	if(stat(dispatch_fifo_name, &st) != 0){
		errno = 0;
		err = mkfifo(dispatch_fifo_name, 0666);
		CrashOnError(err && errno != EEXIST, "Error creating  fifo_names");
	}

	int fd_fifo =  open(dispatch_fifo_name, O_WRONLY|O_APPEND);
	CrashOnError(fd_fifo < 0, "Error getting the descriptor of fifo_names");
	
	int n  = write(fd_fifo, name, NAMELEN);
	CrashOnError(n < 0, "Error writing in fifo_names");
}

void GetFifoName(char name[NAMELEN]){
	struct stat st;
	int err;

	if(stat(dispatch_fifo_name, &st) != 0){
		printf("Error: nothing to print\n");
		exit(1);
	}
	int fd_fifo = open(dispatch_fifo_name, O_RDONLY);
	CrashOnError(fd_fifo<0, "Error getting the descriptor of fifo_names");

	int n = read(fd_fifo, name, NAMELEN);
	// printf("#### name %s ####\n", name);
	CrashOnError(n < 0, "Error reading from fifo");

	err = close(fd_fifo);
	CrashOnError(err, "Error closing fifo");
}

int LoopAndWrite(int fd_fifo, char buf[SIZE], int n) {
	int m;
	for (int i = 0; i < 100000000; i++) {
		errno = 0;
		m = write(fd_fifo, buf, n);
		CrashOnError(m < 0 && errno != 35, "Error writing file");
		if (m > 0) {
			return m;
		}
	}
	CrashOnError(-1, "Error waiting too long to write file");
	return -1;
}

int LoopAndRead(int fd_fifo, char buf [SIZE], int n) {
	int m;
	for (int i = 0; i < 100000000; i++){
		errno = 0;
		m = read(fd_fifo, buf, SIZE);
		CrashOnError(m < 0 && errno != 35, "Error reading from fifo in loop");
		if (m > 0){
			return m;
		}
	}
	CrashOnError(-1, "Error waiting too long to read from fifo");
	return -1;
}

void Transmit(FILE *in) {
	char name[NAMELEN];
	int err;

	CreateIndividualName(name);
	err = mkfifo(name, 0666);
	CrashOnError(err, "Error creating main fifo");

	int fd_fifo = open(name, O_RDWR|O_NONBLOCK);
	CrashOnError(fd_fifo < 0, "Error getting the descriptor of fifo");

	SendName(name);
	int fd_in = fileno(in);
	CrashOnError(fd_in < 0, "Error getting the descriptor of file");

	char buf [SIZE];
	int n = 1;
	int nw;

	while(n > 0) {
		n = read(fd_in, buf, SIZE);
		CrashOnError(n < 0, "Error reading file");

		errno = 0;
		nw = write(fd_fifo, buf, n);
		if (nw < 0 && errno == 35){
			nw = LoopAndWrite(fd_fifo, buf, n);
		}
		CrashOnError(nw < 0, "Error writing file1");
	}
	
	err = close(fd_fifo);
	CrashOnError(err, "Error closing fifo");
	
	err = remove(name);
	CrashOnError(err, "Error removing fifo");

	return;
}

void Recieve() {
	int err;
	char name [NAMELEN];
	GetFifoName(name);

	struct stat st;
	if(stat(name, &st) != 0){
		printf("Error: nothing to print\n");
		exit(1);
	}

 	int fd_fifo = open(name, O_RDONLY|O_NONBLOCK);
	CrashOnError(fd_fifo < 0, "Error getting the descriptor of fifo");
	sleep(1);
	int n = 1;
	int nw;
	char buf [SIZE];
	while(n > 0) {
		errno = 0;
		n = read(fd_fifo, buf, SIZE);
		if (n < 0 && errno == 35){
			n = LoopAndRead(fd_fifo, buf, SIZE);
		}
		CrashOnError(n < 0, "Error reading from fifo");

		nw = write(STDOUT_FILENO, buf, n);
		CrashOnError(nw < 0, "Error writing to stdout");
	} 
	
	err = close(fd_fifo);
	CrashOnError(err, "Error closing fifo");
	
	return;
}



int main(int argc, char** argv) {
	switch (argc) {
		case 1: {
			Recieve();
			break;
		}
		case 2: {
			FILE *in = fopen(argv[1], "rb");
			CrashOnError(!in, "Error opening file");
			Transmit(in);
			break;
		}
		default: {
			printf("Error: invalid number of arguments\n");
			break;
		}
	}

	return 0;
}