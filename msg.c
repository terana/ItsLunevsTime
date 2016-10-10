#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

void CrashOnError (int err, char *descr) {
	if(err != 0){
		perror(descr);
		exit(1);
	}
}

int main(int argc, char** argv) 
{
	if(argc != 2){
                printf("Invalid number of arguments\nUsage: \n%s n\nwhere n is integer value between 1 and %ld\n", argv[0], LONG_MAX);
                return 0;
        }

        char *endptr = NULL;
        errno = 0;
        long n = strtol(argv[1], &endptr, 10);
        if ((errno == ERANGE && (n == LONG_MAX || n == LONG_MIN))
                   || (errno != 0 && n == 0)
                        || endptr != argv[1] + strlen(argv[1])
                                || n < 0){
                printf("The number is not correct\nUsage: \n%s n\n where n is integer value between 0 and %ld\n", argv[0], LONG_MAX);
                return 0;
        }
	
	long i;
	int par_pid = getpid();
	int ch_id = 0;

    int msqid = msgget(ftok("/tmp", 1), (IPC_CREAT | IPC_EXCL| 0666));
    CrashOnError(msqid == -1, "Error initializing msg queue");

	for (i = 0; i < n; i++)
	{
			ch_id = fork();
			if(ch_id == 0) //child
			{
				struct msgbuf { 
        			long mtype;
        			char mtext[1];
     			} msgst;
				
				int msg_size = msgrcv(msqid, &msgst, 1, i + 1, 0);
				CrashOnError(msg_size == -1, "Error recieving message");
				printf("child : %ld\n", i);
				exit(0);
			}

	} 
	int err;
	struct msgbuf { 
        long mtype;
        char *mtext;
     } msgst [n];
	
	for (i = 0; i < n; i++){
		msgst[i].mtype = i + 1;
		err = msgsnd(msqid, msgst + i, 1, 0);
		CrashOnError(err == -1, "Error sending message");
		waitpid(-1, NULL, 0);
	}
	
	err = msgctl(msqid, IPC_RMID, NULL);
	CrashOnError(err == -1, "Error removing message queue");
	return 0;
}