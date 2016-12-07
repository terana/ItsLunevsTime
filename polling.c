//
//  main.c
//  Polling
//
//  Created by Anastasia on 12/4/16.
//  Copyright (c) 2016 terana. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

void CrashOnError(int err, char *descr)
{
	if (err)
	{
		perror(descr);
		exit(1);
	}
}

static int blsize;

struct Pipe
{
	int read_fd;
	int write_fd;
};

struct Buffer
{
	char *buffer;
	int  Capacity;
	int  AvailableToWrite;
	char *CurrentPosition;
};

void NewBuffer(struct Buffer *buf, int capacity)
{
	buf->buffer           = calloc(capacity, sizeof(char));
	buf->Capacity         = capacity;
	buf->AvailableToWrite = 0;
	buf->CurrentPosition  = buf->buffer;
}

void RegisterWritingFromBuffer(struct Buffer *buf, int wr)
{
	CrashOnError(buf->AvailableToWrite < wr, "Written more then available");
	buf->AvailableToWrite -= wr;
	if (buf->AvailableToWrite > 0)
	{
		buf->CurrentPosition += wr;
	}
	else
	{
		buf->CurrentPosition = buf->buffer;
	}
}

void RegisterReadingToBuffer(struct Buffer *buf, int rd)
{
	CrashOnError(rd > buf->Capacity, "Read more then capacity");
	buf->AvailableToWrite = rd;
	buf->CurrentPosition  = buf->buffer;
}

struct Connection
{
	pid_t pid_in;
	pid_t pid_out;

	struct Buffer buf;
	int           read_fd;
	int           write_fd;

	int readyToClose;
};

void PushThrough(int read_fd, int write_fd)
{
	int  err;
	char *buf = malloc(blsize * sizeof(char));
	//printf("In child %d bufer %d\n", getpid(), blsize);
	int rd = 1;
	int wr;
	while (rd > 0)
	{

		rd = read(read_fd, buf, blsize);
		CrashOnError(rd < 0, "Error reading");
	//	printf("Child %d read %d\n", getpid(), rd);

		wr = write(write_fd, buf, rd);
		CrashOnError(wr < 0, "Error writing");
	//	printf("Child %d write %d\n", getpid(), wr);
	}
	err    = close(read_fd);
	CrashOnError(err, "Error closing read_fd in child");
	//printf("closed %d in child\n", read_fd);

	err = close(write_fd);
	CrashOnError(err, "Error closing write_fd in child");
	//printf("closed %d in child\n", write_fd);

	//printf("Child %d exit\n", getpid());
	free(buf);
	return;
}


void FillSets(struct fd_set *write_set, struct fd_set *read_set, struct Connection *conn, long from, long to)
{
	long i;
	FD_ZERO(read_set);
	FD_ZERO(write_set);
	for (i = from; i < to; i++)
	{
		FD_SET(conn[i].read_fd, read_set);
		//	printf("added %d\n", conn[i].read_fd);
		FD_SET(conn[i].write_fd, write_set);
		//printf("added %d\n", conn[i].write_fd);

	}
}

int main(int argc, char const *argv[])
{
	if (argc != 3)
	{
		printf("Error: invalid number of arguments\n");
		exit(1);
	}

	char *endptr = NULL;
	errno = 0;
	long n = strtol(argv[1], &endptr, 10);
	CrashOnError(errno, "The number of children is not correct");

	int fd_in = open(argv[2], O_RDONLY);
	CrashOnError(fd_in < 0, "Error opening file");

	int  err;
	long i;

	struct Connection *conn = calloc(n + 1, sizeof(struct Connection));
	conn[0].read_fd = fd_in;
	int size = 3;
	for (i = n; i >= 0; i--)
	{
		NewBuffer(&conn[i].buf, size);
		size *= 3;
	}
	blsize = size;
	conn[n].write_fd     = STDOUT_FILENO;
	conn[n].readyToClose = 0;

	struct fd_set read_set;
	struct fd_set write_set;

	int         maxfd = 0;
	struct Pipe child_in;
	struct Pipe child_out;

	int ch_pid = 0;
	for (i   = 0; i < n; i++)
	{
		err = pipe((int *) &child_in);
		CrashOnError(err, "Error creating pipe child in");
		err = pipe((int *) &child_out);
		CrashOnError(err, "Error creating pipe child out");

		ch_pid = fork();
//CHILD
		if (ch_pid == 0)
		{
			int j;
			for (j = 0; j < i; j++)
			{
				err = close(conn[j].read_fd);
				CrashOnError(err, "Error closing fd in child");
				err = close(conn[j].write_fd);
				CrashOnError(err, "Error closing fd in child");
			}

			err = close(child_in.write_fd);
			CrashOnError(err, "Error closing pipe in child");
			err = close(child_out.read_fd);
			CrashOnError(err, "Error closing pipe in child");

			PushThrough(child_in.read_fd, child_out.write_fd);

			return 0;
		}
		else
		{
//PARENT
			err = close(child_in.read_fd);
			CrashOnError(err, "Error closing pipe in parent");
			err = close(child_out.write_fd);
			CrashOnError(err, "Error closing pipe in parent");

			conn[i].readyToClose = 0;

			conn[i].pid_out    = ch_pid;
			conn[i + 1].pid_in = ch_pid;

			conn[i].write_fd    = child_in.write_fd;
			conn[i + 1].read_fd = child_out.read_fd;

			fcntl(child_in.write_fd, F_SETFL, O_NONBLOCK);

			if (child_in.write_fd > maxfd)
			{
				maxfd = child_in.write_fd;
			}
			if (child_out.read_fd > maxfd)
			{
				maxfd = child_out.read_fd;
			}
		}
	}

	struct timespec timeout;


	int wr;
	int rd;

	int numberOfReadyFD;
	long tail = -1;
	while (tail < n)
	{
		FillSets(&write_set, &read_set, conn, tail + 1, n + 1);
		timeout.tv_sec  = 0;
		timeout.tv_nsec = 0;
		numberOfReadyFD = pselect(maxfd + 1, &read_set, &write_set, NULL, &timeout, NULL);
		CrashOnError(numberOfReadyFD < 0, "Error in pselect");

		for (i = tail + 1; i <= n; i++)
		{
			if (FD_ISSET(conn[i].read_fd, &read_set))
			{
				if (conn[i].buf.AvailableToWrite == 0)
				{
					rd = read(conn[i].read_fd, conn[i].buf.CurrentPosition, conn[i].buf.Capacity);
					CrashOnError(rd < 0, "Error reading");
					RegisterReadingToBuffer(&conn[i].buf, rd);
					//printf("Parend read from %d %d bytes\n", conn[i].pid_in, rd);
					if (rd == 0)
					{
						conn[i].readyToClose = 1;
					}
				}
			}
		}

		for (i = tail + 1; i <= n; i++)
		{
			if (FD_ISSET(conn[i].write_fd, &write_set))
			{
				if (conn[i].buf.AvailableToWrite > 0)
				{
					wr = write(conn[i].write_fd, conn[i].buf.CurrentPosition, conn[i].buf.AvailableToWrite);
					CrashOnError(wr < 0, "Error writing");
					RegisterWritingFromBuffer(&conn[i].buf, wr);
					//printf("Parend write to %d %d bytes, %d availible\n", conn[i].pid_out, wr, conn[i].buf.AvailableToWrite);
				}
			}
			if (conn[i].readyToClose && conn[i].buf.AvailableToWrite == 0)
			{
				err = close(conn[i].read_fd);
				CrashOnError(err, "Error closing fd in parend");
				err = close(conn[i].write_fd);
				CrashOnError(err, "Error closing fd in parent");
				tail = i;
			}
		}
	}

	for (i = 0; i <= n; i++)
	{
		free(conn[i].buf.buffer);
	}
	free(conn);

	return 0;
}