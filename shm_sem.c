#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>


static const int blsize = 100;
static const int shmemsize = blsize + sizeof(int) + sizeof(char);

static const int ftok_id = 10;
static const char* ftok_path = "/tmp";
static const int wnull_key = 100500;

static const int mut = 0; //the index in semaphore array
static const int empty = 1;
static const int full = 2;

static const char transmitter_status_ok = 't';
static const char transmitter_status_finished = 'e';
static const char reciever_status_ok = 'r';

void CrashOnError(int err, char* descr) {
	if(err){
		perror(descr);
		exit(1);
	}
}

void WakeRecieverUp () {
	int err;
	int wnullid = semget(wnull_key, 1, SEM_A | (SEM_A>>3) | (SEM_A>>6)); // sem0 for wait for null
	CrashOnError(wnullid < 0, "#### semged wnullid failed in transmitter");

	struct sembuf signal = (struct sembuf) {0, -1, SEM_UNDO}; // set wnul = 0
	err = semop(wnullid, &signal, 1);
	CrashOnError(err == -1, "#### semop signal failed in transmitter");

}

void Transmit(FILE *in){
	int err;

	int key = ftok (ftok_path, ftok_id);
	CrashOnError(key == -1, "#### ftok failed in transmitter");
	int shmid = shmget(key, shmemsize, 0666 | IPC_CREAT);
	CrashOnError(shmid == -1, "#### shmget failed in transmitter");
	errno = 0;
	char *shmem = shmat(shmid, 0, 0);
	CrashOnError(errno, "#### shmat failed in transmitter");
	int semid = semget(key, 3, 0666 | IPC_CREAT); // sem0 for mutex, sem1 for empty, sem2 for full
	CrashOnError(semid == -1, "#### semget failed in transmitter");
	
	printf("tr 1\n");

	struct sembuf init[2];
	init[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	init[1] = (struct sembuf) {empty, 1, SEM_UNDO};

	err = semop(semid, init, 2);
	CrashOnError(err, "#### semop init failed in transmitter");
	shmem[0] = '0';

	struct sembuf enter[2];
	enter[0] = (struct sembuf) {empty, -1, SEM_UNDO};
	enter[1] = (struct sembuf) {mut, -1, SEM_UNDO};

	struct sembuf ext[2];
	ext[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	ext[1] = (struct sembuf) {full, 1, SEM_UNDO};

	printf("tr 2\n");

	int fd_in = fileno(in);
	CrashOnError(fd_in < 0, "#### fileno(in) failed in transmitter"); 
	int n = 1;
	int *ptr;
	char rs = reciever_status_ok;
	int first_time = 1;
	while(n > 0) {
		err = semop(semid, enter, 2);
		CrashOnError(err, "#### semop enter failed in transmitter");

		rs = shmem[0];
		if((rs != reciever_status_ok) && (first_time == 0)) {
			printf("#### TOTAL FAIL IN TRANSMITTER: something went wrong with reciever\n");
			exit(1);
		}

		n = read(fd_in, shmem + sizeof(char) + sizeof(int), blsize);
		CrashOnError(n < 0, "#### read from file failed in transmitter");
		
		printf("tr rw %d\n", n);

		ptr = (int *)(shmem + sizeof(char));
		*ptr = n;

		if (n > 0) {
			shmem[0] = transmitter_status_ok;
		} else {
			shmem[0] = transmitter_status_finished;
			printf("TRANSMITTER finished\n");
		}

		err = semop(semid, ext, 2);
		CrashOnError(err, "#### semop exit failed in transmitter");
		if(first_time == 1) {
			WakeRecieverUp();
			printf("tr wake\n");
			first_time = 0;
		}
		//sleep(1);
	}

	err = semop(semid, enter, 2);
	CrashOnError(err, "#### semop enter failed in transmitter");

	rs = shmem[0];
	if((rs != reciever_status_ok) && (first_time == 0)) {
		printf("#### TOTAL FAIL IN TRANSMITTER: something went wrong with reciever\n");
		exit(1);
	}

	err = semop(semid, ext, 2);
	CrashOnError(err, "#### semop exit failed in transmitter");



	printf("tr 5\n");


	err = shmctl(shmid, IPC_RMID, 0); //?????????????
	CrashOnError(err, "#### shmctl removing shm failed in transmitter");
	err = semctl(semid, IPC_RMID, 0);
	CrashOnError(err, "#### semctl removing sems failed in transmitter");
}

void WaitForNull() {
	int err;

	int wnullid = semget(wnull_key, 1, SEM_A | (SEM_A>>3) | (SEM_A>>6) | IPC_CREAT); // sem0 for wait for null
	CrashOnError(wnullid < 0, "#### semged wnullid failed in reciever");

	struct sembuf wnull_init = (struct sembuf) {0, 1, SEM_UNDO}; //wnull = 1
	err = semop(wnullid, &wnull_init, 1);
	CrashOnError(err, "#### semop wnull_init failed in reciever");

	struct sembuf wait = (struct  sembuf) {0, 0, SEM_UNDO}; // wait for wnull == 0
	err = semop(wnullid, &wait, 1);
	CrashOnError(err, "#### semop wait failed in reciever");

	err = semctl(wnullid, 0, IPC_RMID);
	CrashOnError(err == -1, "#### semctl remove failed in transmitter");
}

void Recieve() {
	int err;
	
	WaitForNull();
	
	printf("rc 1\n");

	int key = ftok (ftok_path, ftok_id);
	CrashOnError(key == -1, "#### ftok failed in reciever");
	int shmid = shmget(key, shmemsize, 0);
	CrashOnError(shmid == -1, "#### shmget failed in reciever");
	errno = 0;
	char *shmem = shmat(shmid, 0, 0);
	CrashOnError(errno, "#### shmat failed in reciever");
	int semid = semget(key, 3, 0);
	CrashOnError(semid == -1, "#### semget failed in reciever");

	struct sembuf enter[2];
	enter[0] = (struct sembuf) {full, -1, SEM_UNDO};
	enter[1] = (struct sembuf) {mut, -1, SEM_UNDO};

	struct sembuf ext[2];
	ext[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	ext[1] = (struct sembuf) {empty, 1, SEM_UNDO};

	printf("rc 2\n");

	char ts = transmitter_status_ok;
	int n = 0;
	int *ptr;
	while(ts == transmitter_status_ok) {
		err = semop(semid, enter, 2);
		CrashOnError(err, "#### semop enter failed in reciever");

		printf("rc 5\n");

		ts = shmem[0];
		if ((ts != transmitter_status_ok) && (ts != transmitter_status_finished)) {
			printf("TOTAL FAIL IN RECIEVER: something went wrong with transmitter\n");
			exit(1);
		} 
		if (ts == transmitter_status_ok) {
			ptr = (int *) (shmem + sizeof(char));
			n = *ptr;
			n = write(STDOUT_FILENO, shmem + sizeof(int) + sizeof(char), n);
			CrashOnError(n < 0, "#### write to stdout failed in reciever");
		}
		shmem[0] = reciever_status_ok;
		err = semop(semid, ext, 2);
		CrashOnError(err, "#### semop exit failed in reciever");		
	}
	printf("rc 6\n");
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
			int err = fclose(in);
			CrashOnError(err, "Error closing file");
			break;
		}
		default: {
			printf("Error: invalid number of arguments\n");
			break;
		}
	}

	return 0;
}