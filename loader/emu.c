// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <asm/ucontext.h>

#include "header.h"

#define iolog printf
//#define iolog(...)

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

static int memdev;
static volatile u16 *memregs, *blitter;


static volatile void *translate_addr(u32 a, u32 *mask)
{
  if ((a & 0xfff00000) == 0x7f000000) {
    *mask = 0xffff;
    return memregs;
  }
  if ((a & 0xfff00000) == 0x7f100000) {
    *mask = 0xff;
    return blitter;
  }
  fprintf(stderr, "bad IO @ %08x\n", a);
  abort();
}

static u32 xread8(u32 a)
{
  volatile u8 *mem;
  u32 mask;

  iolog("r8  %08x\n", a);
  mem = translate_addr(a, &mask);
  return mem[a & mask];
}

static u32 xread16(u32 a)
{
  volatile u16 *mem;
  u32 mask;

  iolog("r16 %08x\n", a);
  mem = translate_addr(a, &mask);
  return mem[(a & mask) / 2];
}

static u32 xread32(u32 a)
{
  volatile u32 *mem;
  u32 mask;

  iolog("r32 %08x\n", a);
  mem = translate_addr(a, &mask);
  return mem[(a & mask) / 4];
}

static void xwrite8(u32 a, u32 d)
{
  volatile u8 *mem;
  u32 mask;

  iolog("w8  %08x %08x\n", a, d);
  mem = translate_addr(a, &mask);
  mem[a & mask] = d;
}

static void xwrite16(u32 a, u32 d)
{
  volatile u16 *mem;
  u32 mask;

  iolog("w16 %08x %08x\n", a, d);
  mem = translate_addr(a, &mask);
  mem[(a & mask) / 2] = d;
}

static void xwrite32(u32 a, u32 d)
{
  volatile u32 *mem;
  u32 mask;

  iolog("w32 %08x %08x\n", a, d);
  mem = translate_addr(a, &mask);
  mem[(a & mask) / 4] = d;
}

#define BIT_SET(v, b) (v & (1 << (b)))

static void handle_op(u32 addr_pc, u32 op, u32 *regs, u32 addr_check)
{
  u32 t, shift, ret, addr;
  int rn, rd;

  rd = (op & 0x0000f000) >> 12;
  rn = (op & 0x000f0000) >> 16;

  if ((op & 0x0f200090) == 0x01000090) { // AM3: LDRH, STRH
    if (BIT_SET(op, 6)) // S
      goto unhandled;

    if (BIT_SET(op, 22))                // imm offset
      t = ((op & 0xf00) >> 4) | (op & 0x0f);
    else                                // reg offset
      t = regs[op & 0x000f];

    if (!BIT_SET(op, 23))
      t = -t;
    addr = regs[rn] + t;

    if (BIT_SET(op, 20)) { // Load
      ret = xread16(addr);
      regs[rd] = ret;
    }
    else
      xwrite16(addr, regs[rd]);
  }
  else if ((op & 0x0d200000) == 0x05000000) { // AM2: LDR[B], STR[B]
    if (BIT_SET(op, 25)) {              // reg offs
      if (BIT_SET(op, 4))
        goto unhandled;

      t = regs[op & 0x000f];
      shift = (op & 0x0f80) >> 7;
      switch ((op & 0x0060) >> 5) {
        case 0: t = t << shift; break;
        case 1: t = t >> (shift + 1); break;
        case 2: t = (signed int)t >> (shift + 1); break;
        case 3: goto unhandled; // I'm just lazy
      }
    }
    else                                // imm offs
      t = op & 0x0fff;

    if (!BIT_SET(op, 23))
      t = -t;
    addr = regs[rn] + t;

    if (BIT_SET(op, 20)) { // Load
      if (BIT_SET(op, 22)) // Byte
        ret = xread8(addr);
      else
        ret = xread32(addr);
      regs[rd] = ret;
    }
    else {
      if (BIT_SET(op, 22)) // Byte
        xwrite8(addr, regs[rd]);
      else
        xwrite32(addr, regs[rd]);
    }
  }
  else
    goto unhandled;

  if (addr != addr_check) {
    fprintf(stderr, "bad calculated addr: %08x vs %08x\n", addr, addr_check);
    abort();
  }
  return;

unhandled:
  fprintf(stderr, "unhandled IO op %08x @ %08x\n", op, addr_pc);
}

static void segv_sigaction(int num, siginfo_t *info, void *ctx)
{
  struct ucontext *context = ctx;
  u32 *regs = (u32 *)&context->uc_mcontext.arm_r0;
  u32 op = *(u32 *)regs[15];

  //printf("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);
/*
  static int thissec, sfps;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  sfps++;
  if (tv.tv_sec != thissec) {
    printf("%d\n", sfps);
    sfps = 0;
    thissec = tv.tv_sec;
  }
*/

  handle_op(regs[15], op, regs, (u32)info->si_addr);
  regs[15] += 4;
  return;

  //signal(num, SIG_DFL);
  //raise(num);
}

#define LINKPAGE_SIZE 0x1000

struct linkpage {
  u32 (*xread8)(u32 a);
  u32 (*xread16)(u32 a);
  u32 (*xread32)(u32 a);
  void (*xwrite8)(u32 a, u32 d);
  void (*xwrite16)(u32 a, u32 d);
  void (*xwrite32)(u32 a, u32 d);
  u32 retval;
  u32 *reg_ptr;
  u32 saved_regs[6]; // r0-r3,r12,lr
  u32 code[0];
};

static struct linkpage *g_linkpage;
static u32 *g_code_ptr;

void emu_init(void *map_bottom)
{
  struct sigaction segv_action = {
    .sa_sigaction = segv_sigaction,
    .sa_flags = SA_SIGINFO,
  };
  struct linkpage init_linkpage = {
    .xread8  = xread8,
    .xread16 = xread16,
    .xread32 = xread32,
    .xwrite8  = xwrite8,
    .xwrite16 = xwrite16,
    .xwrite32 = xwrite32,
  };
  void *ret;

  sigemptyset(&segv_action.sa_mask);
  sigaction(SIGSEGV, &segv_action, NULL);

  g_linkpage = (void *)(((u32)map_bottom - LINKPAGE_SIZE) & ~0xfff);
  ret = mmap(g_linkpage, LINKPAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (ret != g_linkpage) {
    perror("mmap linkpage");
    exit(1);
  }
  printf("linkpage @ %p\n", g_linkpage);
  memcpy(g_linkpage, &init_linkpage, sizeof(*g_linkpage));
  g_linkpage->reg_ptr = g_linkpage->saved_regs;
  g_code_ptr = g_linkpage->code;

  memdev = open("/dev/mem", O_RDWR);
  memregs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
  blitter = mmap(NULL, 0x100, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xe0020000);
  printf("mapped %d %p %p\n", memdev, memregs, blitter);
}

void *emu_mmap_dev(unsigned int length, int prot, int flags, unsigned int offset)
{
  char name[32];
  int fd;

  if ((offset & ~0xffff) == 0xc0000000) {
    return mmap((void *)0x7f000000, length, PROT_NONE,
      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_NORESERVE, -1, 0);
  }
  if ((offset & ~0xffff) == 0xe0020000) {
    return mmap((void *)0x7f100000, length, PROT_NONE,
      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_NORESERVE, -1, 0);
  }
  // pass through
  if ((offset & 0xfe000000) == 0x02000000)
    return mmap(NULL, length, prot, flags, memdev, offset);

  sprintf(name, "m%08x", offset);
  fd = open(name, O_CREAT|O_RDWR, 0644);
  lseek(fd, length - 1, SEEK_SET);
  name[0] = 0;
  write(fd, name, 1);

  return mmap(NULL, length, prot, MAP_SHARED, fd, 0);
}

