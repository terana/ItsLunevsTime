#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


static const int blsize = 100;
static unsigned char byte = 0;
static unsigned char offset = 1;
static const unsigned char max_offset = (1 << 7);
static int ch_pid;
static unsigned char buf [100];
static int i = 0;


void CrashOnError(int err, char* descr) {
	if(err){
		perror(descr);
		exit(1);
	}
}

void PrintToStdout() {
	int n = write(STDOUT_FILENO, buf, i);
	CrashOnError(n < 0, "Error writing to stdout");
	fflush(stdout);
}

void RememberByte() {
	buf[i++] = byte;
	byte = 0;
	if(i == blsize) {
		PrintToStdout();
		i = 0;
	}
}

void GotOne(int signumber) {
	byte += offset;
	offset = (offset << 1);

	int err = kill(ch_pid, SIGUSR1);
	CrashOnError(err < 0, "Error telling child OK");
}

void GotZero(int signumber) {
	offset = (offset << 1);
	
	int err = kill(ch_pid, SIGUSR1);
	CrashOnError(err < 0, "Error telling child OK");
}

void GotChildIsDead (int signumber) {
	if(i > 0) {
		i--;
	}
	PrintToStdout();
	exit(0);
}

void GotParentIsDead(int signumber) {
	printf("Error: PARENT IS DEAD\n");
	exit(1);
}

void GotParentIsAlive(int signumber) {
	alarm(0);
}

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Error: invalid number of arguments\nUsage %s file\n", argv[0]);
		exit(1);
	}

	int err;

	sigset_t empty_set;
	err = sigemptyset(&empty_set);
	CrashOnError(err, "Error creating empty set");

	sigset_t block_set;
	err = sigemptyset(&block_set);
	CrashOnError(err < 0, "Error initializing block_set in parent");
	err = sigaddset(&block_set, SIGUSR1);
	CrashOnError(err < 0, "Error adding SIGUSR1 to block_set");
	err = sigaddset(&block_set, SIGUSR2);
	CrashOnError(err < 0, "Error adding SIGUSR2 to block_set");
	err = sigaddset(&block_set, SIGCHLD);
	CrashOnError(err < 0, "Error adding SIGCHILD to block_set");
	err = sigaddset(&block_set, SIGALRM);
		CrashOnError(err <0, "Error adding SIGALRM to child set");
	err = sigprocmask(SIG_BLOCK, &block_set, NULL);
	CrashOnError(err < 0, "Error changing signal mask of parent");

	ch_pid = fork();
//CHILD
	if(ch_pid == 0) {
		int fd_in = open(argv[1], O_RDONLY);
		CrashOnError(fd_in < 0, "Error opening file");

		int par_pid = getppid();

		struct sigaction pdead_act;
		pdead_act.sa_handler = GotParentIsDead;
		pdead_act.sa_flags = 0;
		err = sigfillset(&pdead_act.sa_mask);
		CrashOnError(err, "Error filling set for sigaction pdead_act");
		err = sigaction(SIGALRM, &pdead_act, NULL);
		CrashOnError(err < 0, "Error setting action GotParentIsDead");

		struct sigaction palive_act;
		palive_act.sa_handler = GotParentIsAlive;
		palive_act.sa_flags = 0;
		err = sigfillset(&palive_act.sa_mask);
		CrashOnError(err, "Error filling set for sigaction palive_act");
		err = sigaction(SIGUSR1, &palive_act, NULL);
		CrashOnError(err < 0, "Error setting action GotParentIsAlive");
		
		int n = 1;
		while (n > 0) {
			n = read(fd_in, &byte, 1);
			CrashOnError(n < 0, "Error reading from file");
			for (offset = 1; offset > 0; offset = (offset << 1)) {
				if (offset & byte) { 
					err = kill(par_pid, SIGUSR1);
					CrashOnError(err < 0, "Error sending one(SIGUSR1) to parent");
				} else {
					err = kill(par_pid, SIGUSR2);
					CrashOnError(err < 0, "Error sending zero (SIGUSR2) to parent");
				}
				alarm(1);
				sigsuspend(&empty_set);
			}
			//printf("send %c\n", byte);
		}

		err = close(fd_in);
		CrashOnError(err != 0, "Error closing file");
		exit(0);
	} 
//PARENT
	struct sigaction one_act;
	one_act.sa_handler = GotOne;
	one_act.sa_flags = 0;
	err = sigfillset(&one_act.sa_mask);
	CrashOnError(err, "Error filling set for sigaction one_act");
	err = sigaction(SIGUSR1, &one_act, NULL);
	CrashOnError(err < 0, "Error setting action GotOne");

	struct sigaction zero_act;
	zero_act.sa_handler = GotZero;
	zero_act.sa_flags = 0;
	err = sigfillset(&zero_act.sa_mask);
	CrashOnError(err, "Error filling set for sigaction zero_act");
	err = sigaction(SIGUSR2, &zero_act, NULL);
	CrashOnError(err < 0, "Error setting action GotZero");	

	struct sigaction chdead_act;
	chdead_act.sa_handler = GotChildIsDead;
	chdead_act.sa_flags = 0;
	err = sigfillset(&chdead_act.sa_mask);
	CrashOnError(err, "Error filling set for sigaction chdead_act");
	err = sigaction(SIGCHLD, &chdead_act, NULL);
	CrashOnError(err < 0, "Error setting action GotChildIsDead");

	while(1) {
		sigsuspend(&empty_set);
		if (offset == 0){
			offset = 1;
			RememberByte();
		}
	}
	
	return 0;
}