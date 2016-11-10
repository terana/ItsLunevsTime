#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

static const int blsize = 100;
static const int shmsize = blsize + sizeof(int) + sizeof(char);

static const int ftok_id = 12;
static const char* ftok_path = "/tmp";
static const int ftok_tr_id = 5;
static const int ftok_rec_id = 6;

static const int mut = 0; //the index in semaphore array
static const int empty = 1;
static const int full = 2;

static const char transmitter_status_ok = 't';
static const char reciever_status_ok = 'r';

void CrashOnError(int err, char* descr) {
	if(err){
		perror(descr);
		exit(1);
	}
}

int GetTransmitterSemId() {
	int key = ftok (ftok_path, ftok_tr_id);
	CrashOnError(key == -1, "#### ftok for tr semid failed"); 
	int semid = semget(key, 1, 0666 | IPC_CREAT);
	CrashOnError(semid < 0, "#### semget failed for tr semid");

	// int val = semctl(semid, 0, GETVAL);
	// printf("tr sem val  %d | id %d\n", val, semid);
	return semid;
}

int GetRecieverSemId() {
	int key = ftok (ftok_path, ftok_rec_id);
	CrashOnError(key == -1, "#### ftok for rec semid failed"); 
	int semid = semget(key, 1, 0666 | IPC_CREAT);
	CrashOnError(semid < 0, "#### semget failed for rec semid");

	// int val = semctl(semid, 0, GETVAL);
	// printf("rec sem val  %d | id %d\n", val, semid);
	return semid;
}

char *GetShmem() {
	int key = ftok (ftok_path, ftok_id);
	CrashOnError(key == -1, "#### ftok for shmem failed"); 
	int shmid = shmget(key, shmsize, 0666 | IPC_CREAT);
	CrashOnError(shmid == -1, "#### shmget for shmem failed");
	errno = 0;
	char *shmem = shmat(shmid, 0, 0);
	CrashOnError(errno, "#### shmat for shmem failed");
	return shmem;
}

int GetSharedSemId() {
	int key = ftok (ftok_path, ftok_id);
	CrashOnError(key == -1, "#### ftok for semid failed"); 
	int semid = semget(key, 3, 0666 | IPC_CREAT); // sem0 for mutex, sem1 for empty, sem2 for full

	return semid;
}

void SetOne(int semid) {
	int err = semctl(semid, 0, SETVAL, 1);
	CrashOnError(err < 0, "#### semctl failed setting 1 to sem");
}

void Producer_Enter(int semid) {
	struct sembuf enter[2];
	enter[0] = (struct sembuf) {empty, -1, 0};
	enter[1] = (struct sembuf) {mut, -1, SEM_UNDO};

	int err = semop(semid, enter, 2);
	CrashOnError(err, "#### semop enter failed in transmitter");
	
	// unsigned short values [3];
	// err = semctl(semid, 0, GETALL, values);
	// printf ("pr enter mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void Producer_Exit(int semid) {
	int err;
	struct sembuf ext[2];
	ext[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	ext[1] = (struct sembuf) {full, 1, 0};

	err = semop(semid, ext, 2);
	CrashOnError(err, "#### semop exit failed in transmitter");

	// unsigned short values [3];
	// err = semctl(semid, 0, GETALL, values);
	// printf ("pr exit mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void Consumer_Enter (int semid) {
	struct sembuf enter[2];
	enter[0] = (struct sembuf) {full, -1, 0};
	enter[1] = (struct sembuf) {mut, -1, SEM_UNDO};

	int err = semop(semid, enter, 2);
	CrashOnError(err, "#### semop enter failed in reciever");

	// unsigned short values [3];
	// err = semctl(semid, 0, GETALL, values);
	// printf ("con enter mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void Consumer_Exit (int semid) {
	int err;

	struct sembuf ext[2];
	ext[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	ext[1] = (struct sembuf) {empty, 1, 0};

	err = semop(semid, ext, 2);
	CrashOnError(err, "#### semop exit failed in reciever");

	// unsigned short values [3];
	// err = semctl(semid, 0, GETALL, values);
	// printf ("con exit mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void TrInit (int semid) {
	int err;
	unsigned short values [3] = {1, 1, 0};
	err = semctl(semid, 0, SETALL, values);
	CrashOnError(err, "#### semctl init failed");

	struct sembuf init [2];
	init [0] =  (struct sembuf) {full, 2, 0};
	init [1] = (struct sembuf) {full, -2, SEM_UNDO};
	err = semop(semid, init, 2);
	CrashOnError(err, "#### semop init failed in transmitter");
	//printf("tr init\n");
}

void RecInit (int semid) {
	int err;

	struct sembuf init [2];
	init[0] = (struct sembuf) {empty, 2, 0};
	init[1] =  (struct sembuf) {empty, -2, SEM_UNDO};
	err = semop(semid, init, 2);
	CrashOnError(err, "#### semop init failed in reciever");
	//printf("rec init\n");
}


void Transmit (FILE *in) {
	int err;

	int tr_semid = GetTransmitterSemId();
	int pid = semctl(tr_semid, 0, GETPID);
	CrashOnError(pid < 0, "#### semctl failed getting pid of last transmitter");
	if (pid == 0) {
		SetOne(tr_semid);
	}
	struct sembuf enter = {0, -1, SEM_UNDO};
	err = semop(tr_semid, &enter, 1);
	CrashOnError(err < 0, "#### error entering sem");

	int semid = GetSharedSemId();
	char *shmem = GetShmem();
	TrInit(semid);

	int rec_semid = GetRecieverSemId();
	SetOne(rec_semid);

	int fd_in = fileno(in);
	CrashOnError(fd_in < 0, "#### fileno(in) failed in transmitter"); 
	int n = 1;
	int *ptr;
	char rs = reciever_status_ok;
	int first_time = 1;
	while(n > 0) {
		Producer_Enter(semid);

		rs = shmem[0];
		if((rs != reciever_status_ok) && (first_time == 0)) {
			printf("#### TOTAL FAIL IN TRANSMITTER: something went wrong with reciever\n");
			exit(1);
		}

		n = read(fd_in, shmem + sizeof(char) + sizeof(int), blsize);
		CrashOnError(n < 0, "#### read from file failed in transmitter");
	
		ptr = (int *)(shmem + sizeof(char));
		*ptr = n;

		shmem[0] = transmitter_status_ok;
		if(first_time){
			first_time = 0;
		}
		Producer_Exit(semid);
	}
}

void Recieve() {
	int err;

	int rec_semid = GetRecieverSemId();

	struct sembuf enter = {0, -1, 0};
	err = semop(rec_semid, &enter, 1);
	CrashOnError(err < 0, "#### error entering sem");


	char *shmem = GetShmem();
	int semid = GetSharedSemId();

	RecInit(semid);

	char ts = transmitter_status_ok;
	int n = 1;
	int wres;
	int *ptr;
	while(n > 0) {
		Consumer_Enter(semid);

		ts = shmem[0];
		ptr = (int *) (shmem + sizeof(char));
		n = *ptr;

		if (ts != transmitter_status_ok) {
			printf("TOTAL FAIL IN RECIEVER: something went wrong with transmitter\n");
			exit(1);
		} 
		if(n > 0) {
			wres = write(STDOUT_FILENO, shmem + sizeof(int) + sizeof(char), n);
			CrashOnError(wres < 0, "#### write to stdout failed in reciever");
		}
		
		shmem[0] = reciever_status_ok;
		Consumer_Exit(semid);	
	}
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