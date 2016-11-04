#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

int main () {
	mkfifo("fifo", 666);
	int fd = open("fifo", O_RDWR);
	int n = write(fd, "aaa", 3);
	close(fd);
	fd = open("fifo", O_RDONLY|O_NONBLOCK);O_WRONLY | O_NONBLOCK
	int buf[3] = {1, 2, 3};
	write(1, buf, read(fd, buf, 3));
	return 0;
}