// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "header.h"
#include "sys_cacheflush.h"

#if 0
#define strace printf
#else
#define strace(...)
#endif

// note: first mask must be always full for search algo
static const unsigned int sig_mask_all[] = {
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

static const unsigned int sig_open[] = {
  0xe59cc000, 0xe33c0000, 0x1a000003, 0xef900005
};
#define sig_mask_open sig_mask_all

static const unsigned int sig_mmap[] = {
  0xe92d000f, 0xe1a0000d, 0xef90005a, 0xe28dd010
};
#define sig_mask_mmap sig_mask_all

static const unsigned int sig_mmap_[] = {
  0xe52d5004, 0xe59d5008, 0xe52d4004, 0xe59d4008,
  0xe1b0ca05, 0x1a000006, 0xe1a05625, 0xef9000c0
};
#define sig_mask_mmap_ sig_mask_all

static const unsigned int sig_read[] = {
  0xe59fc080, 0xe59cc000, 0xe33c0000, 0x1a000003, 0xef900003
};
#define sig_mask_read sig_mask_all

static const unsigned int sig_ioctl[] = {
  0xef900036, 0xe3700a01, 0x312fff1e
};
#define sig_mask_ioctl sig_mask_all

static const unsigned int sig_sigaction[] = {
  0xe59f300c, 0xe3530000, 0x0a000000, 0xea000000, 0xea000000
};
static const unsigned int sig_mask_sigaction[] = {
  0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000
};

static const struct dev_fd_t takeover_devs[] = {
  { "/dev/mem",     FAKEDEV_MEM },
  { "/dev/GPIO",    FAKEDEV_GPIO },
  { "/dev/fb0",     FAKEDEV_FB0 },
  { "/dev/fb/0",    FAKEDEV_FB0 },
  { "/dev/fb1",     FAKEDEV_FB1 },
  { "/dev/fb/1",    FAKEDEV_FB1 },
  { "/dev/mmuhack", FAKEDEV_MMUHACK },
};

static int w_open(const char *pathname, int flags, mode_t mode)
{
  int i, ret;

  for (i = 0; i < ARRAY_SIZE(takeover_devs); i++) {
    if (strcmp(pathname, takeover_devs[i].name) == 0) {
      ret = takeover_devs[i].fd;
      break;
    }
  }

  if (i == ARRAY_SIZE(takeover_devs))
    ret = open(pathname, flags, mode);

  if (ret >= 0) {
    for (i = 0; emu_interesting_fds[i].name != NULL; i++) {
      if (strcmp(pathname, emu_interesting_fds[i].name) == 0) {
        emu_interesting_fds[i].fd = ret;
        break;
      }
    }
  }

  strace("open(%s) = %d\n", pathname, ret);
  return ret;
}

static void *w_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  void *ret;
  if (FAKEDEV_MEM <= fd && fd < FAKEDEV_FD_BOUNDARY)
    ret = emu_do_mmap(length, prot, flags, fd, offset);
  else
    ret = mmap(addr, length, prot, flags, fd, offset);

  // threads are using heap before they mmap their stack
  // printf needs valid stack for pthtead/errno
  if (((long)&ret & 0xf0000000) == 0xb0000000)
    strace("mmap(%p, %x, %x, %x, %d, %lx) = %p\n", addr, length, prot, flags, fd, (long)offset, ret);
  return ret;
}
#define w_mmap_ w_mmap

static ssize_t w_read(int fd, void *buf, size_t count)
{
  ssize_t ret;
  if (fd != FAKEDEV_GPIO)
    return read(fd, buf, count);

  ret = emu_read_gpiodev(buf, count);
  //strace("read(%d, %p, %d) = %d\n", fd, buf, count, ret);
  return ret;
}

static int w_ioctl(int fd, int request, void *argp)
{
  int ret;

  if ((FAKEDEV_MEM <= fd && fd < FAKEDEV_FD_BOUNDARY) ||
      fd == emu_interesting_fds[IFD_SOUND].fd)
    ret = emu_do_ioctl(fd, request, argp);
  else
    ret = ioctl(fd, request, argp);

  strace("ioctl(%d, %08x, %p) = %d\n", fd, request, argp, ret);
  return ret;
}

static int w_sigaction(int signum, const void *act, void *oldact)
{
  strace("sigaction(%d, %p, %p) = %d\n", signum, act, oldact, 0);
  return 0;
}

#define PATCH(f) { sig_##f, sig_mask_##f, ARRAY_SIZE(sig_##f), w_##f }

static const struct {
  const unsigned int *sig;
  const unsigned int *sig_mask;
  size_t sig_cnt;
  void *func;
} patches[] = {
  PATCH(open),
  PATCH(mmap),
  PATCH(mmap_), // mmap using mmap2 syscall
  PATCH(read),
  PATCH(ioctl),
  PATCH(sigaction),
};

void do_patches(void *ptr, unsigned int size)
{
  int i, s;

  for (i = 0; i < ARRAY_SIZE(patches); i++) {
    const unsigned int *sig = patches[i].sig;
    const unsigned int *sig_mask = patches[i].sig_mask;
    unsigned int *seg = (void *)(((long)ptr + 3) & ~3);
    unsigned int *seg_end = seg + size / 4;
    unsigned int sig0 = sig[0];

    for (; seg < seg_end; seg++) {
      if (*seg != sig0)
        continue;

      for (s = 1; s < patches[i].sig_cnt; s++)
        if ((seg[s] ^ sig[s]) & sig_mask[s])
          break;

      if (s == patches[i].sig_cnt)
        goto found;
    }
    continue;

found:
    dbg("  patch #%i @ %08x\n", i, (int)seg);
    seg[0] = 0xe59ff000; // ldr pc, [pc]
    seg[1] = 0;
    seg[2] = (unsigned int)patches[i].func;
  }

  sys_cacheflush(ptr, (char *)ptr + size);
}

