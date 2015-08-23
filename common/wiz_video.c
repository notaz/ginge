/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
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
#include "warm/warm.h"

static volatile unsigned short *memregs;
static volatile unsigned int   *memregl;
int probably_caanoo;
int memdev = -1;

#define FB_BUF_COUNT 4
static unsigned int fb_paddr[FB_BUF_COUNT];
static int fb_buf_count = FB_BUF_COUNT;
static int fb_work_buf;
static int fbdev = -1;

static void *gp2x_screens[FB_BUF_COUNT];
static void *g_screen_ptr;


static void vout_gp2x_flip(void)
{
	memregl[0x406C>>2] = fb_paddr[fb_work_buf];
	memregl[0x4058>>2] |= 0x10;

	fb_work_buf++;
	if (fb_work_buf >= fb_buf_count)
		fb_work_buf = 0;
	g_screen_ptr = gp2x_screens[fb_work_buf];
}

static int vout_gp2x_init(int no_dblbuf)
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
	probably_caanoo = fb_paddr[0] >= 0x4000000;
	printf("looking like Caanoo? %s.\n", probably_caanoo ? "yes" : "no");

	gp2x_screens[0] = mmap(0, 320*240*2*FB_BUF_COUNT, PROT_READ|PROT_WRITE,
			MAP_SHARED, memdev, fb_paddr[0]);
	if (gp2x_screens[0] == MAP_FAILED)
	{
		perror("mmap(gp2x_screens) failed");
		exit(1);
	}
	memset(gp2x_screens[0], 0, 320*240*2*FB_BUF_COUNT);

	if (!no_dblbuf) {
		warm_init();
		ret = warm_change_cb_range(WCB_B_BIT, 1, gp2x_screens[0], 320*240*2*FB_BUF_COUNT);
		if (ret != 0)
			fprintf(stderr, "could not make fb buferable.\n");
	}

	// printf("  %p -> %08x\n", gp2x_screens[0], fb_paddr[0]);
	for (i = 1; i < FB_BUF_COUNT; i++)
	{
		fb_paddr[i] = fb_paddr[i-1] + 320*240*2;
		gp2x_screens[i] = (char *)gp2x_screens[i-1] + 320*240*2;
		// printf("  %p -> %08x\n", gp2x_screens[i], fb_paddr[i]);
	}
	fb_work_buf = 0;
	g_screen_ptr = gp2x_screens[0];

	if (no_dblbuf)
		fb_buf_count = 1;

	return 0;
}

static void vout_gp2x_set_mode(int bpp, int rot)
{
	int rot_cmd[2] = { 0, 0 };
	int code = 0, bytes = 2;
	unsigned int r;
	int ret;

	if (probably_caanoo)
		rot = 0;

	rot_cmd[0] = rot ? 6 : 5;
	ret = ioctl(fbdev, _IOW('D', 90, int[2]), rot_cmd);
	if (ret < 0)
		perror("rot ioctl failed");

	memregl[0x4004>>2] = rot ? 0x013f00ef : 0x00ef013f;
	memregl[0x4000>>2] |= 1 << 3;

	switch (bpp)
	{
		case 8:
			code = 0x443a;
			bytes = 1;
			break;

		case 15:
		case 16:
			code = 0x4432;
			bytes = 2;
			break;

		default:
			fprintf(stderr, "unhandled bpp request: %d\n", abs(bpp));
			return;
	}

	memregl[0x405c>>2] = bytes;
	memregl[0x4060>>2] = bytes * (rot ? 240 : 320);

	r = memregl[0x4058>>2];
	r = (r & 0xffff) | (code << 16) | 0x10;
	memregl[0x4058>>2] = r;
}

static void vout_gp2x_set_palette16(unsigned short *pal, int len)
{
	int i;
	for (i = 0; i < len; i++)
		memregl[0x4070>>2] = (i << 24) | pal[i];
}

static void vout_gp2x_set_palette32(unsigned int *pal, int len)
{
	/* pollux palette is 16bpp only.. */
	int i;
	for (i = 0; i < len; i++)
	{
		int c = pal[i];
		c = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07c0) | ((c >> 3) & 0x001f);
		memregl[0x4070>>2] = (i << 24) | c;
	}
}

void vout_gp2x_finish(void)
{
	if (memregl != NULL) {
		if (memregl[0x4058>>2] & 0x10)
			usleep(100000);
		if (memregl[0x4058>>2] & 0x10)
			printf("MLCCONTROL1 dirty? %08x %08x\n",
				memregl[0x406C>>2], memregl[0x4058>>2]);

		memregl[0x406C>>2] = fb_paddr[0];
		memregl[0x4058>>2] |= 0x10;
		munmap((void *)memregs, 0x20000);
		memregs = NULL;
		memregl = NULL;
	}

	close(fbdev);
	close(memdev);

	warm_finish();
}

