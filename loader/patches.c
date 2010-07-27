// vim:shiftwidth=2:expandtab
#include <stdio.h>

#include "header.h"
#include "sys_cacheflush.h"

#include "override.c"

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

static const unsigned int sig_mmap2[] = {
  0xe52d5004, 0xe59d5008, 0xe52d4004, 0xe59d4008,
  0xe1b0ca05, 0x1a000006, 0xe1a05625, 0xef9000c0
};
#define sig_mask_mmap2 sig_mask_all

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


#define PATCH(f) { sig_##f, sig_mask_##f, ARRAY_SIZE(sig_##f), w_##f }

static const struct {
  const unsigned int *sig;
  const unsigned int *sig_mask;
  size_t sig_cnt;
  void *func;
} patches[] = {
  PATCH(open),
  PATCH(mmap),
  PATCH(mmap2), // mmap2 syscall
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

