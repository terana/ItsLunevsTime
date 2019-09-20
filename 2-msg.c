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
	int ch_id = 0;

    int msqid = msgget(IPC_PRIVATE, (IPC_CREAT  | IPC_EXCL | 0666));
    CrashOnError(msqid == -1, "Error initializing msg queue");

    int err;
	
    struct msgbuf { 
        long mtype;
        char *mtext;
    };

	struct msgbuf msgtorcv;
	struct msgbuf msgtosnd;

	for (i = 0; i < n; i++)
	{
			ch_id = fork();
			if(ch_id == 0) //child
			{
				//printf("child %ld is waiting for msg\n", i);
				err = msgrcv(msqid, &msgtorcv, 0, i + 1, 0);
				CrashOnError(err == -1, "Error recieving message in child");
				printf("child : %ld\n", i);

				msgtosnd.mtype = i +  n + 1;
				err = msgsnd(msqid, &msgtosnd, 0, 0);
				CrashOnError(err == -1, "Error sending message in child");

				//printf("child %ld has send\n", i);
	
				exit(0);
			}

	} 
	
	
	for (i = 0; i < n; i++){
		//printf("parent is sending to %ld child\n", i);
		msgtosnd.mtype = i + 1;
		err = msgsnd(msqid, &msgtosnd, 0, 0);
		CrashOnError(err == -1, "Error sending message in parent");
		
		//printf("par has send to %ld child and is waiting to rcv\n", i);
		err = msgrcv(msqid, &msgtorcv, 0, i + n + 1, 0);
		CrashOnError(err == -1, "Error recieving message in parent");
		//printf("par has recieved from %ld child\n", i);
	}
	
	err = msgctl(msqid, IPC_RMID, NULL);
	CrashOnError(err == -1, "Error removing message queue");
	return 0;
}
