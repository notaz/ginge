#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>

int main(int argc, char *argv[])
{
	volatile void *memregs;
	int memdev;
	int i;

	printf("hi, home=%s\n", getenv("HOME"));

	for (i = 0; i < argc; i++)
		printf("%d \"%s\"\n", i, argv[i]);

	memdev = open("/dev/mem", O_RDWR);
	if (memdev < 0) {
		perror("open");
		return 1;
	}

	memregs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
	ioctl(-1, 0);
	signal(7, SIG_DFL);

	for (i = 0; i < 2; i++)
		printf("%02x %04x %08x\n", ((char *)memregs)[0x2011],
			((short *)memregs)[0x1198/2], ((int *)memregs)[0xbcdc/4]);

	//sleep(1000);

	return 0;
}

