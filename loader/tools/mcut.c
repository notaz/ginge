#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE *fi;
	int c, t;

	if (argc < 4) {
		printf("usage:\n%s <file> <offs> <count>\n", argv[0]);
		return 1;
	}

	fi = fopen(argv[1], "rb");
	fseek(fi, strtoul(argv[2], NULL, 0), SEEK_SET);
	c = atoi(argv[3]);

	while (c--) {
		fread(&t, 1, 4, fi);
		printf("0x%08x, ", t);
	}
	printf("\n");

	return 0;
}

