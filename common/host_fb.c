// vim:shiftwidth=2:expandtab
#include <string.h>
#ifdef LOADER
#include "../loader/realfuncs.h"
#endif

#include "host_fb.h"

static void *host_screen;
static int host_stride;

#if defined(PND)

#include "fbdev.c"

static struct vout_fbdev *fbdev;

void *host_video_flip(void)
{
  host_screen = vout_fbdev_flip(fbdev);
  return host_screen;
}

int host_video_init(int *stride, int no_dblbuf)
{
  const char *fbdev_name;
  int w, h;

  fbdev_name = getenv("FBDEV");
  if (fbdev_name == NULL)
    fbdev_name = "/dev/fb1";

  fbdev = vout_fbdev_init(fbdev_name, &w, &h, no_dblbuf);
  if (fbdev == NULL)
    return -1;

  host_stride = w * 2;
  if (stride != 0)
    *stride = host_stride;
  host_video_flip();

  return 0;
}

#elif defined(WIZ)

#include "warm.c"
#include "wiz_video.c"

void *host_video_flip(void)
{
  vout_gp2x_flip();
  host_screen = g_screen_ptr;
  return host_screen;
}

int host_video_init(int *stride, int no_dblbuf)
{
  int ret;

  host_stride = 320 * 2;
  if (stride != 0)
    *stride = host_stride;

  ret = vout_gp2x_init(no_dblbuf);
  if (ret != 0)
    return ret;

  host_video_flip();
  return 0;
}

#endif

static unsigned short host_pal[256];

static void host_update_pal(unsigned int *pal)
{
  unsigned short *dstp = host_pal;
  int i;

  for (i = 0; i < 256; i++, pal++, dstp++) {
    unsigned int t = *pal;
    *dstp = ((t >> 8) & 0xf800) | ((t >> 5) & 0x07e0) | ((t >> 3) & 0x001f);
  }
}

void host_video_blit4(const unsigned char *src, int w, int h, unsigned int *pal)
{
  unsigned short *dst = host_screen;
  unsigned short *hpal = host_pal;
  int i, u;

  if (pal != NULL)
    host_update_pal(pal);

  for (i = 0; i < 240; i++, dst += host_stride / 2 - 320) {
    for (u = 320 / 2; u > 0; u--, src++) {
      *dst++ = hpal[*src >> 4];
      *dst++ = hpal[*src & 0x0f];
    }
  }

  host_video_flip();
}

void host_video_blit8(const unsigned char *src, int w, int h, unsigned int *pal)
{
  unsigned short *dst = host_screen;
  unsigned short *hpal = host_pal;
  int i, u;

  if (pal != NULL)
    host_update_pal(pal);

  for (i = 0; i < 240; i++, dst += host_stride / 2 - 320) {
    for (u = 320 / 4; u > 0; u--) {
      *dst++ = hpal[*src++];
      *dst++ = hpal[*src++];
      *dst++ = hpal[*src++];
      *dst++ = hpal[*src++];
    }
  }

  host_video_flip();
}

void host_video_blit16(const unsigned short *src, int w, int h)
{
  unsigned short *dst = host_screen;
  int i;

  for (i = 0; i < 240; i++, dst += host_stride / 2, src += 320)
    memcpy(dst, src, 320*2);

  host_video_flip();
}

