#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>


static const int blsize = 100;
static const int shmemsize = blsize + sizeof(int) + sizeof(char);
static const int dispatch_shmsize = 5; //for pid_t

static const int ftok_dispatch_id = 29;
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

void Init (int semid) {
	unsigned short values [3] = {1, 1, 0};
	int err = semctl(semid, 0, SETALL, values);
	CrashOnError(err, "#### semctl init failed");
	printf("init\n");
}

void Producer_Enter(int semid) {
	struct sembuf enter[2];
	enter[0] = (struct sembuf) {empty, -1, SEM_UNDO};
	enter[1] = (struct sembuf) {mut, -1, SEM_UNDO};

	int err = semop(semid, enter, 2);
	CrashOnError(err, "#### semop enter failed in transmitter");
	unsigned short values [3];
	err = semctl(semid, 0, GETALL, values);
	printf ("pr enter mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void Producer_Exit(int semid) {
	struct sembuf ext[2];
	ext[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	ext[1] = (struct sembuf) {full, 1, SEM_UNDO};

	int err = semop(semid, ext, 2);
	CrashOnError(err, "#### semop exit failed in transmitter");
	unsigned short values [3];
	err = semctl(semid, 0, GETALL, values);
	printf ("pr exit mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void Consumer_Enter (int semid) {
	struct sembuf enter[2];
	enter[0] = (struct sembuf) {full, -1, SEM_UNDO};
	enter[1] = (struct sembuf) {mut, -1, SEM_UNDO};

	int err = semop(semid, enter, 2);
	CrashOnError(err, "#### semop enter failed in reciever");

	unsigned short values [3];
	err = semctl(semid, 0, GETALL, values);
	printf ("con enter mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}

void Consumer_Exit (int semid) {
	int err;

	struct sembuf ext[2];
	ext[0] = (struct sembuf) {mut, 1, SEM_UNDO};
	ext[1] = (struct sembuf) {empty, 1, SEM_UNDO};

	err = semop(semid, ext, 2);
	CrashOnError(err, "#### semop exit failed in reciever");
	//Init(semid);
	unsigned short values [3];
	err = semctl(semid, 0, GETALL, values);
	printf ("con exit mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], semid);
}


int *GetDispatcherShmem() {
	int key = ftok (ftok_path, ftok_dispatch_id);
	CrashOnError(key == -1, "#### ftok for dispatcher shmem failed"); 
	int dispatch_shmid = shmget(key, dispatch_shmsize, 0666 | IPC_CREAT);
	CrashOnError(dispatch_shmid == -1, "#### shmget for dispatcher shmem failed");
	errno = 0;
	int *shmem = shmat(dispatch_shmid, 0, 0);
	CrashOnError(errno, "#### shmat for dispatcher shmem failed");
	return shmem;
}

int GetDispatcherSemId() {
	int key = ftok (ftok_path, ftok_dispatch_id);
	CrashOnError(key == -1, "#### ftok for dispatcher semid failed"); 
	errno = 0;
	int dispatch_semid = semget(key, 3, 0666 | IPC_CREAT | IPC_EXCL); // sem0 for mutex, sem1 for empty, sem2 for full
	if (errno == EEXIST) {
		dispatch_semid = semget(key, 3, 0666);
		CrashOnError(dispatch_semid == -1, "#### dispatcher semget failed, sem exist");
	} else {
		CrashOnError(dispatch_semid == -1, "#### dispatcher semget failed");
		Init(dispatch_semid);
	}

	unsigned short values [3];
	int err = semctl(dispatch_semid, 0, GETALL, values);
	printf ("dispatch sems : mut %d | empty %d | full %d | id %d\n", values[0], values[1], values [2], dispatch_semid);
	return dispatch_semid;
}

void BroadcastPid() {
	int *dispatch_shmem = GetDispatcherShmem();
	int dispatch_semid = GetDispatcherSemId();

	Producer_Enter(dispatch_semid);
	*dispatch_shmem = getpid();
	Producer_Exit(dispatch_semid);
	printf("BroadcastPid %d\n", getpid());
}

void Transmit(FILE *in){
	int err;

	int key = ftok (ftok_path, getpid());
	CrashOnError(key == -1, "#### ftok failed in transmitter"); 
	int shmid = shmget(key, shmemsize, 0666 | IPC_CREAT);
	CrashOnError(shmid == -1, "#### shmget failed in transmitter");
	errno = 0;
	char *shmem = shmat(shmid, 0, 0);
	CrashOnError(errno, "#### shmat failed in transmitter");
	int semid = semget(key, 3, 0666 | IPC_CREAT); // sem0 for mutex, sem1 for empty, sem2 for full
	CrashOnError(semid == -1, "#### semget failed in transmitter");
	Init(semid);
	
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
			BroadcastPid();
		}
		Producer_Exit(semid);
	}

	Producer_Enter(semid);
	rs = shmem[0];
	if(rs != reciever_status_ok) {
		printf("#### TOTAL FAIL IN TRANSMITTER: something went wrong with reciever in the end\n");
		exit(1);
	}
	shmem[0] = transmitter_status_ok;
	Producer_Exit(semid);

	err = shmctl(shmid, IPC_RMID, 0); 
	CrashOnError(err, "#### shmctl removing shm failed in transmitter");
	err = semctl(semid, IPC_RMID, 0);
	CrashOnError(err, "#### semctl removing sems failed in transmitter");
}

int GetTransmitterPid(){
	int *dispatch_shmem =  GetDispatcherShmem();
	int dispatch_semid =  GetDispatcherSemId();

	Consumer_Enter (dispatch_semid);
	int pid = *dispatch_shmem;
	Consumer_Exit (dispatch_semid);

	printf("got pid %d\n", pid);
	return(pid);
}

void Recieve() {
	int err;

	int pid = GetTransmitterPid();

	int key = ftok (ftok_path, pid);
	CrashOnError(key == -1, "#### ftok failed in reciever");
	int shmid = shmget(key, shmemsize, 0);
	CrashOnError(shmid == -1, "#### shmget failed in reciever");
	errno = 0;
	char *shmem = shmat(shmid, 0, 0);
	CrashOnError(errno, "#### shmat failed in reciever");
	int semid = semget(key, 3, 0);
	CrashOnError(semid == -1, "#### semget failed in reciever");

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