/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011,2016
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#include <stdio.h>

#include "header.h"
#include "syscalls.h"

#include "override.c"

// note: first mask int must be always full for the search algo
static const unsigned int sig_mask_all[] = {
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

static const unsigned int sig_open[] = {
  0xe59cc000, // ldr ip, [ip]
  0xe33c0000, // teq ip, #0
  0x1a000003, // bne 0x1c
  0xef900005, // svc 0x900005
};
#define sig_mask_open sig_mask_all

static const unsigned int sig_open_a1[] = {
  0xef900005, // svc 0x900005
  0xe1a0f00e, // mov pc, lr
};
#define sig_mask_open_a1 sig_mask_all

static const unsigned int sig_mmap[] = {
  0xe92d000f, // push {r0, r1, r2, r3}
  0xe1a0000d, // mov  r0, sp
  0xef90005a, // svc  0x90005a
  0xe28dd010, // add  sp, sp, #16
};
#define sig_mask_mmap sig_mask_all

static const unsigned int sig_munmap[] = {
  0xef90005b, // svc  0x90005b
  0xe3700a01, // cmn  r0, #0x1000
  0x312fff1e, // bxcc lr
};
#define sig_mask_munmap sig_mask_all

static const unsigned int sig_mmap2[] = {
  0xe52d5004, // push {r5}
  0xe59d5008, // ldr  r5, [sp, #8]
  0xe52d4004, // push {r4}
  0xe59d4008, // ldr  r4, [sp, #8]
  0xe1b0ca05, // lsls ip, r5, #20
  0x1a000006, // bne  0x34
  0xe1a05625, // lsr  r5, r5, #12
  0xef9000c0, // svc  0x9000c0
};
#define sig_mask_mmap2 sig_mask_all

static const unsigned int sig_read[] = {
  0xe59fc080, // ldr ip, [pc, #128]
  0xe59cc000, // ldr ip, [ip]
  0xe33c0000, // teq ip, #0
  0x1a000003, // bne 0x20
  0xef900003, // svc 0x900003
};
#define sig_mask_read sig_mask_all

static const unsigned int sig_read_a1[] = {
  0xef900003, // svc  0x900003
  0xe3700a01, // cmn  r0, #0x1000
  0x312fff1e, // bxcc lr
};
#define sig_mask_read_a1 sig_mask_all

static const unsigned int sig_hw_read[] = {
  0xef900003, // svc  0x900003
  0xe3700a01, // cmn  r0, #0x1000
  0xe1a04000, // mov  r4, r0
};
#define sig_mask_hw_read sig_mask_all

static const unsigned int sig_ioctl[] = {
  0xef900036, // svc  0x900036
  0xe3700a01, // cmn  r0, #0x1000
  0x312fff1e, // bxcc lr
};
#define sig_mask_ioctl sig_mask_all

static const unsigned int sig_hw_ioctl[] = {
  0xef900036, // svc  0x900036
  0xe3700a01, // cmn  r0, #0x1000
  0xe1a04000, // mov  r4, r0
};
#define sig_mask_hw_ioctl sig_mask_all

static const unsigned int sig_sigaction[] = {
  0xe59f300c, //    ldr r3, [pc, #12]
  0xe3530000, //    cmp r3, #0
  0x0a000000, //    beq 0f
  0xea000000, //    b   *
  0xea000000, // 0: b   *
};
static const unsigned int sig_mask_sigaction[] = {
  0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000
};

static const unsigned int sig_execve[] = {
  0xef90000b, // svc 0x90000b
  0xe1a04000, // mov r4, r0
  0xe3700a01, // cmn r0, #4096
};
#define sig_mask_execve sig_mask_all

static const unsigned int sig_execve2[] = {
  0xef90000b, // svc 0x90000b
  0xe3700a01, // cmn r0, #4096
  0xe1a04000, // mov r4, r0
};
#define sig_mask_execve2 sig_mask_all

static const unsigned int sig_chdir[] = {
  0xef90000c, // svc  0x90000c
  0xe3700a01, // cmn  r0, #4096
  0x312fff1e, // bxcc lr
  0xea0004bb, // b    *
};
static const unsigned int sig_mask_chdir[] = {
  0xffffffff, 0xffffffff, 0xffffffff, 0xff000000
};

/* special */
static const unsigned int sig_cache1[] = {
  0xee073f5e, // mcr 15, 0, r3, cr7, cr14, 2
};
#define sig_mask_cache1 sig_mask_all

static const unsigned int sig_cache2[] = {
  0xee070f17, // mcr 15, 0, r0, cr7, cr7, 0
};
#define sig_mask_cache2 sig_mask_all

/* additional wrappers for harder case of syscalls within the code stream */
#ifdef PND /* fix PC, not needed on ARM9 */
# define SVC_CMN_R0_MOV_R4_PC_ADJ() \
"  ldr  r12, [sp, #5*4]\n" \
"  add  r12, r12, #4\n" \
"  str  r12, [sp, #5*4]\n"
#else
# define SVC_CMN_R0_MOV_R4_PC_ADJ()
#endif

#define SVC_CMN_R0_MOV_R4_WRAPPER(name, target) \
extern int name(); \
asm( \
#name ":\n" \
"  stmfd sp!, {r1-r3,r12,lr}\n" \
   SVC_CMN_R0_MOV_R4_PC_ADJ() \
"  bl   " #target "\n" \
"  cmn  r0, #0x1000\n" \
"  mov  r4, r0\n" \
"  ldmfd sp!, {r1-r3,r12,lr,pc}\n" \
);

SVC_CMN_R0_MOV_R4_WRAPPER(hw_read, w_read_raw)
SVC_CMN_R0_MOV_R4_WRAPPER(hw_ioctl, w_ioctl_raw)

#define PATCH_(p, f, t) { sig_##p, sig_mask_##p, ARRAY_SIZE(sig_##p), t, f, #p }
#define PATCH(f) PATCH_(f, w_##f, 0)

static const struct {
  const unsigned int *sig;
  const unsigned int *sig_mask;
  size_t sig_cnt;
  unsigned int type;
  void *func;
  const char *name;
} patches[] = {
  PATCH (open),
  PATCH_(open_a1, w_open, 0),
  PATCH (mmap),
  PATCH (mmap2), // mmap2 syscall
  PATCH (munmap),
  PATCH (read),
  PATCH_(read_a1, w_read, 0),
  PATCH_(hw_read, hw_read, 1),
  PATCH (ioctl),
  PATCH_(hw_ioctl, hw_ioctl, 1),
  PATCH (sigaction),
//  PATCH_(execve, execve2, 0), // hangs
  PATCH (chdir),
  PATCH_(cache1, NULL, 2),
  PATCH_(cache2, NULL, 2),
};

void do_patches(void *ptr, unsigned int size)
{
  unsigned int *seg = (void *)(((long)ptr + 3) & ~3);
  unsigned int *seg_end = seg + size / 4;
  int i, s;

  for (; seg < seg_end; seg++) {
    for (i = 0; i < ARRAY_SIZE(patches); i++) {
      const unsigned int *sig = patches[i].sig;
      const unsigned int *sig_mask;

      if (*seg != sig[0])
        continue;

      sig_mask = patches[i].sig_mask;
      for (s = 1; s < patches[i].sig_cnt; s++)
        if ((seg[s] ^ sig[s]) & sig_mask[s])
          break;

      if (s == patches[i].sig_cnt) {
        switch (patches[i].type) {
        case 0:
          seg[0] = 0xe51ff004; // ldr   pc, [pc, #-4]
          seg[1] = (unsigned int)patches[i].func;
          break;
        case 1:
          seg[0] = 0xe92d8000; // stmfd sp!, {pc}
          seg[1] = 0xe51ff004; // ldr   pc, [pc, #-4]
          seg[2] = (unsigned int)patches[i].func;
          break;
        case 2:
          if (seg < (unsigned int *)ptr + 1 || (seg[-1] >> 28) != 0x0e)
            // might be data
            continue;
          seg[0] = 0xe1a00000; // nop
          break;
        default:
          err("bad patch type: %u\n", patches[i].type);
          abort();
        }
        dbg("  patch #%2i @ %08x type %d %s\n",
          i, (int)seg, patches[i].type, patches[i].name);
        seg += patches[i].sig_cnt - 1;
        break;
      }
    }
  }

  sys_cacheflush(ptr, (char *)ptr + size);
}

// vim:shiftwidth=2:expandtab
