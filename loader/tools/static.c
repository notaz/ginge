#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int main()
{
	volatile void *memregs;
	int memdev;
	int i;

	printf("hi\n");

	memdev = open("/dev/mem", O_RDWR);
	memregs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);

	for (i = 0; i < 2; i++)
		printf("%02x %04x %08x\n", ((char *)memregs)[0x2011],
			((short *)memregs)[0x1198/2], ((int *)memregs)[0xbcdc/4]);

	//sleep(1000);

	return 0;
}

