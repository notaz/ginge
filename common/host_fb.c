/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#include <string.h>
#ifdef LOADER
#include "../loader/realfuncs.h"
#endif

#include "host_fb.h"

static void *host_screen;
static int host_stride;

#if defined(PND)

#include "libpicofe/linux/fbdev.c"
#include "omapfb.h"

static struct vout_fbdev *fbdev;
static unsigned short host_pal[256];

static int get_layer_size(int *x, int *y, int *w, int *h)
{
  struct omapfb_plane_info pi;
  int ret;

  ret = ioctl(vout_fbdev_get_fd(fbdev), OMAPFB_QUERY_PLANE, &pi);
  if (ret != 0) {
    perror("OMAPFB_QUERY_PLANE");
    return -1;
  }

  *x = pi.pos_x;
  *y = pi.pos_y;
  *w = pi.out_width;
  *h = pi.out_height;
  printf("layer: %d,%d %dx%d\n", *x, *y, *w, *h);

  return 0;
}

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
  fbdev = vout_fbdev_init(fbdev_name, &w, &h, 16, no_dblbuf ? 1 : 3);
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

void host_video_normalize_ts(int *x1024, int *y1024)
{
  static int lx, ly, lw = 800, lh = 480, checked;

  if (!checked) {
    get_layer_size(&lx, &ly, &lw, &lh);
    checked = 1; // XXX: might change, so may need to recheck
  }
  *x1024 = (*x1024 - lx) * 1024 / lw;
  *y1024 = (*y1024 - ly) * 1024 / lh;
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

void host_video_normalize_ts(int *x1024, int *y1024)
{
  *x1024 = *x1024 * 1024 / 320;
  *y1024 = *y1024 * 1024 / 240;
}

#endif // WIZ

// vim:shiftwidth=2:expandtab
