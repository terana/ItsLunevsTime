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
static char *magicmessage = "terana is awesome";

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

void Transmit(FILE *in) {
	char name[NAMELEN];
	int err;

	CreateIndividualName(name);
	err = mkfifo(name, 0666);
	CrashOnError(err, "Error creating unique fifo");

	int fd_fifo1 = open(name, O_RDWR|O_NONBLOCK);
	CrashOnError(fd_fifo1 < 0, "Error opening unique fifo the first time");

	SendName(name);
	//exit(1);
	int fd_in = fileno(in);
	CrashOnError(fd_in < 0, "Error getting the descriptor of file");
	
	struct stat st;
	err = fstat(fd_in, &st);
	CrashOnError(err, "Error getting info about input file");
	unsigned long long file_size = st.st_size;

	char buf [SIZE];
	int n = 1;
	int nw;
	
	//exit(1);
	int fd_fifo2 = open(name, O_WRONLY);
	CrashOnError(fd_fifo2 < 0, "Error opening unique fifo the second time");
	err = fcntl(fd_fifo2, F_SETNOSIGPIPE, 0);
	CrashOnError(err, "Error enabling sigpipe");

	sleep(3);
	err = close(fd_fifo1);
	CrashOnError(err, "Error closing the rdwr descriptor");

	//exit(1);

	nw = write(fd_fifo2, &file_size, sizeof(unsigned long long));
	CrashOnError(nw < 0, "Error writing in unique fifo the size of file");
	while(n > 0) {
		n = read(fd_in, buf, SIZE);
		CrashOnError(n < 0, "Error reading file");

		nw = write(fd_fifo2, buf, n);		
		CrashOnError(nw < 0, "Error writing in unique fifo");
	}

	err = close(fd_fifo2);
	CrashOnError(err, "Error closing second fifo descriptor");
	
	err = remove(name);
	CrashOnError(err, "Error removing unique fifo");

	return;
}

void Recieve() {
	int err;
	char name [NAMELEN];
	GetFifoName(name);
	//exit(1);
	//sleep(2);
	
 	int fd_fifo = open(name, O_RDONLY|O_NONBLOCK);
	CrashOnError(fd_fifo < 0, "Error getting the descriptor of fifo to read");
	//exit(1);
	int n = 1;
	int nw;
	char buf [SIZE];

	err = fcntl(fd_fifo, F_SETFL, 0);
	CrashOnError(err, "Error going to Block mode in reciever");

	unsigned long long file_size;
	n = read(fd_fifo, &file_size, sizeof (unsigned long long));
	unsigned long long act_size = 0;

	while(n > 0) {
		n = read(fd_fifo, buf, SIZE);
		CrashOnError(n < 0, "Error reading from unique fifo");
		act_size += n;
		//exit(1);
		nw = write(STDOUT_FILENO, buf, n);
		CrashOnError(nw < 0, "Error writing to stdout");
	} 
	
	err = close(fd_fifo);
	CrashOnError(err, "Error closing unique fifo in reader");

	if (act_size != file_size) {
		printf("\nDon't belive this information! \nIt is not the correct file! \nIt seems like something went wrong during the transfering the data\n");
		exit(1);
	}

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