#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>

static volatile unsigned short *memregs;
static volatile unsigned long  *memregl;
static int memdev = -1;

#define FB_BUF_COUNT 4
static unsigned int fb_paddr[FB_BUF_COUNT];
static int fb_work_buf;
static int fbdev = -1;

static void *gp2x_screens[FB_BUF_COUNT];
static void *g_screen_ptr;


static void vout_gp2x_flip(void)
{
	memregl[0x406C>>2] = fb_paddr[fb_work_buf];
	memregl[0x4058>>2] |= 0x10;

	fb_work_buf++;
	if (fb_work_buf >= FB_BUF_COUNT)
		fb_work_buf = 0;
	g_screen_ptr = gp2x_screens[fb_work_buf];
}

static int vout_gp2x_init(void)
{
	struct fb_fix_screeninfo fbfix;
	int i, ret;

	memdev = open("/dev/mem", O_RDWR);
	if (memdev == -1) {
		perror("open(/dev/mem) failed");
		exit(1);
	}

	memregs	= mmap(0, 0x20000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
	if (memregs == MAP_FAILED) {
		perror("mmap(memregs) failed");
		exit(1);
	}
	memregl = (volatile void *)memregs;

	fbdev = open("/dev/fb0", O_RDWR);
	if (fbdev < 0) {
		perror("can't open fbdev");
		exit(1);
	}

	ret = ioctl(fbdev, FBIOGET_FSCREENINFO, &fbfix);
	if (ret == -1) {
		perror("ioctl(fbdev) failed");
		exit(1);
	}

	printf("framebuffer: \"%s\" @ %08lx\n", fbfix.id, fbfix.smem_start);
	fb_paddr[0] = fbfix.smem_start;

	gp2x_screens[0] = mmap(0, 320*240*2*FB_BUF_COUNT, PROT_READ|PROT_WRITE,
			MAP_SHARED, memdev, fb_paddr[0]);
	if (gp2x_screens[0] == MAP_FAILED)
	{
		perror("mmap(gp2x_screens) failed");
		exit(1);
	}
	memset(gp2x_screens[0], 0, 320*240*2*FB_BUF_COUNT);

	printf("  %p -> %08x\n", gp2x_screens[0], fb_paddr[0]);
	for (i = 1; i < FB_BUF_COUNT; i++)
	{
		fb_paddr[i] = fb_paddr[i-1] + 320*240*2;
		gp2x_screens[i] = (char *)gp2x_screens[i-1] + 320*240*2;
		printf("  %p -> %08x\n", gp2x_screens[i], fb_paddr[i]);
	}
	fb_work_buf = 0;
	g_screen_ptr = gp2x_screens[0];

	return 0;
}

void vout_gp2x_finish(void)
{
	memregl[0x406C>>2] = fb_paddr[0];
	memregl[0x4058>>2] |= 0x10;
	close(fbdev);

	munmap((void *)memregs, 0x20000);
	close(memdev);
}

