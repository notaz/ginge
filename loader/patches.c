// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "header.h"
#include "sys_cacheflush.h"

static const unsigned int sig_open[] = {
  0xe59cc000, 0xe33c0000, 0x1a000003, 0xef900005
};

static const unsigned int sig_mmap[] = {
  0xe92d000f, 0xe1a0000d, 0xef90005a, 0xe28dd010
};

static const unsigned int sig_mmap_[] = {
  0xe52d5004, 0xe59d5008, 0xe52d4004, 0xe59d4008,
  0xe1b0ca05, 0x1a000006, 0xe1a05625, 0xef9000c0
};

#define FAKE_DEVMEM_DEVICE 10001

static int w_open(const char *pathname, int flags, mode_t mode)
{
  int ret;
  if (strcmp(pathname, "/dev/mem") != 0)
    ret = open(pathname, flags, mode);
  else
    ret = FAKE_DEVMEM_DEVICE;

  printf("open(%s) = %d\n", pathname, ret);
  return ret;
}

static void *w_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  void *ret;
  if (fd != FAKE_DEVMEM_DEVICE)
    ret = mmap(addr, length, prot, flags, fd, offset);
  else
    ret = emu_mmap_dev(length, prot, flags, offset);

  printf("mmap(%p, %x, %x, %x, %d, %lx) = %p\n", addr, length, prot, flags, fd, (long)offset, ret);
  return ret;
}
#define w_mmap_ w_mmap

#define PATCH(f) { sig_##f, ARRAY_SIZE(sig_##f), w_##f }

static const struct {
  const unsigned int *sig;
  size_t sig_cnt;
  void *func;
} patches[] = {
  PATCH(open),
  PATCH(mmap),
  PATCH(mmap_), // mmap using mmap2 syscall
};

void do_patches(void *ptr, unsigned int size)
{
  int i, s;

  for (i = 0; i < ARRAY_SIZE(patches); i++) {
    const unsigned int *sig = patches[i].sig;
    unsigned int *seg = (void *)(((long)ptr + 3) & ~3);
    unsigned int *seg_end = seg + size / 4;
    unsigned int sig0 = sig[0];

    for (; seg < seg_end; seg++) {
      if (*seg != sig0)
        continue;

      for (s = 1; s < patches[i].sig_cnt; s++)
        if (seg[s] != sig[s])
          break;

      if (s == patches[i].sig_cnt)
        goto found;
    }
    continue;

found:
    printf("  patch #%i @ %08x\n", i, (int)seg);
    seg[0] = 0xe59ff000; // ldr pc, [pc]
    seg[1] = 0;
    seg[2] = (unsigned int)patches[i].func;
  }

  sys_cacheflush(ptr, (char *)ptr + size);
}

