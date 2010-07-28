#ifdef LOADER
#include "../loader/realfuncs.h"
#endif

#include "fbdev.c"
#include "host_fb.h"

static struct vout_fbdev *fbdev;

int host_video_init(int *stride, int no_dblbuf)
{
	const char *fbdev_name;
	int w, h;

	fbdev_name = getenv("FBDEV");
	if (fbdev_name == NULL)
		fbdev_name = "/dev/fb1";
	
	fbdev = vout_fbdev_init(fbdev_name, &w, &h, no_dblbuf);
	*stride = w * 2;
	return (fbdev != 0) ? 0 : -1;
}

void *host_video_flip(void)
{
	return vout_fbdev_flip(fbdev);
}
