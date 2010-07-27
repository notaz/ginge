#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>

static int open_(const char *name)
{
	int fd = open(name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: ", name);
		perror("open");
		return 1;
	}

	return fd;
}

int main(int argc, char *argv[])
{
	volatile void *memregs;
	void *fbmem;
	int memdev, fbdev;
	int i;

	printf("hi, home=%s\n", getenv("HOME"));

	for (i = 0; i < argc; i++)
		printf("%d \"%s\"\n", i, argv[i]);

	memdev = open_("/dev/mem");
	fbdev = open_("/dev/fb0");

	memregs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
	fbmem = mmap(NULL, 320*240*2, PROT_READ|PROT_WRITE, MAP_SHARED, fbdev, 0);

	ioctl(-1, 0);
	signal(7, SIG_DFL);
//	tcgetattr(-1, NULL);
//	tcsetattr(-1, 0, NULL);

#if 1
	for (i = 0; i < 2; i++)
		printf("%02x %04x %08x\n", ((char *)memregs)[0x2011],
			((short *)memregs)[0x1198/2], ((int *)memregs)[0xbcdc/4]);
#endif
	memset(fbmem, 0xff, 320*240*2);

	sleep(10);

	return 0;
}

