// vim:shiftwidth=2:expandtab
#include <string.h>
#ifdef LOADER
#include "../loader/realfuncs.h"
#endif

#include "host_fb.h"

static void *host_screen;
static int host_stride;

#if defined(PND)

#include "linux/fbdev.c"

static struct vout_fbdev *fbdev;
static unsigned short host_pal[256];

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

  w = h = 0;
  fbdev = vout_fbdev_init(fbdev_name, &w, &h, 16, no_dblbuf);
  if (fbdev == NULL)
    return -1;

  host_stride = w * 2;
  if (stride != 0)
    *stride = host_stride;
  host_video_flip();

  return 0;
}

void host_video_finish(void)
{
  vout_fbdev_finish(fbdev);
  fbdev = NULL;
}

void host_video_update_pal16(unsigned short *pal)
{
  memcpy(host_pal, pal, sizeof(host_pal));
}

void host_video_update_pal32(unsigned int *pal)
{
  unsigned short *dstp = host_pal;
  int i;

  for (i = 0; i < 256; i++, pal++, dstp++) {
    unsigned int t = *pal;
    *dstp = ((t >> 8) & 0xf800) | ((t >> 5) & 0x07e0) | ((t >> 3) & 0x001f);
  }
}

void host_video_change_bpp(int bpp)
{
}

void host_video_blit4(const unsigned char *src, int w, int h, int stride)
{
  unsigned short *dst = host_screen;
  unsigned short *hpal = host_pal;
  int i, u;

  for (i = 0; i < 240; i++, dst += host_stride / 2, src += stride) {
    for (u = 0; i < w / 2; u++) {
      dst[u*2 + 0] = hpal[src[u] >> 4];
      dst[u*2 + 1] = hpal[src[u] & 0x0f];
    }
  }

  host_video_flip();
}

void host_video_blit8(const unsigned char *src, int w, int h, int stride)
{
  unsigned short *dst = host_screen;
  unsigned short *hpal = host_pal;
  int i, u;

  for (i = 0; i < 240; i++, dst += host_stride / 2, src += stride) {
    for (u = 0; u < w; u += 4) {
      dst[u + 0] = hpal[src[u + 0]];
      dst[u + 1] = hpal[src[u + 1]];
      dst[u + 2] = hpal[src[u + 2]];
      dst[u + 3] = hpal[src[u + 3]];
    }
  }

  host_video_flip();
}

void host_video_blit16(const unsigned short *src, int w, int h, int stride)
{
  unsigned short *dst = host_screen;
  int i;

  for (i = 0; i < 240; i++, dst += host_stride / 2, src += stride / 2)
    memcpy(dst, src, w*2);

  host_video_flip();
}

#elif defined(WIZ)

#include "warm/warm.c"
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

  vout_gp2x_set_mode(16, !no_dblbuf);
  host_video_flip();
  return 0;
}

void host_video_finish(void)
{
  vout_gp2x_finish();
}

void host_video_update_pal16(unsigned short *pal)
{
  vout_gp2x_set_palette16(pal, 256);
}

void host_video_update_pal32(unsigned int *pal)
{
  vout_gp2x_set_palette32(pal, 256);
}

void host_video_change_bpp(int bpp)
{
  vout_gp2x_set_mode(bpp, 1);
}

#ifdef LOADER
void host_video_blit4(const unsigned char *src, int w, int h, int stride)
{
  memcpy(host_screen, src, 320*240/2); // FIXME
  host_video_flip();
}

void host_video_blit8(const unsigned char *src, int w, int h, int stride)
{
  if (probably_caanoo) {
    unsigned char *dst = host_screen;
    int i;
    for (i = 0; i < 240; i++, dst += 320, src += stride)
      memcpy(dst, src, w);
  }
  else {
    extern void rotated_blit8(void *dst, const void *linesx4);
    rotated_blit8(host_screen, src);
  }

  host_video_flip();
}

void host_video_blit16(const unsigned short *src, int w, int h, int stride)
{
  if (probably_caanoo) {
    unsigned short *dst = host_screen;
    int i;
    for (i = 0; i < 240; i++, dst += 320, src += stride / 2)
      memcpy(dst, src, w*2);
  }
  else {
    extern void rotated_blit16(void *dst, const void *linesx4);
    rotated_blit16(host_screen, src);
  }

  host_video_flip();
}
#endif // LOADER

#endif // WIZ

