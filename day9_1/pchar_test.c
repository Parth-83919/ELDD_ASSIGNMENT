#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
	int fd, i;
	char str[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";
	fd = open("/dev/pchar0", O_RDWR);
	for(i=0; i<32; i++) {
		write(fd, &str[i], 1);
		printf("written: %c\n", str[i]);
		sleep(1);
	}
	close(fd);
	return 0;
}
