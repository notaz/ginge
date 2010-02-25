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
#include "sys_cacheflush.h"

//#define iolog printf
#define iolog(...)

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

 //if ((a & 0xfff00000) == 0x7f100000) { static int a; a ^= ~1; return a & 0xffff; }
  iolog("r16 %08x\n", a);
  mem = translate_addr(a, &mask);
  return mem[(a & mask) / 2];
}

static u32 xread32(u32 a)
{
  volatile u32 *mem;
  u32 mask;

 //if ((a & 0xfff00000) == 0x7f100000) { static int a; a ^= ~1; return a; }
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

static void handle_op(u32 pc, u32 op, u32 *regs, u32 addr_check)
{
  u32 t, shift, ret, addr;
  int rn, rd;

  rd = (op & 0x0000f000) >> 12;
  rn = (op & 0x000f0000) >> 16;

  if ((op & 0x0f200090) == 0x01000090) { // AM3: LDRH, STRH
    if (!BIT_SET(op, 5)) // !H
      goto unhandled;
    if (BIT_SET(op, 6) && !BIT_SET(op, 20)) // S && !L
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
      if (BIT_SET(op, 6)) { // S
        ret <<= 16;
        ret = (signed int)ret >> 16;
      }
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

#if 0
  if (addr != addr_check) {
    fprintf(stderr, "bad calculated addr: %08x vs %08x\n", addr, addr_check);
    abort();
  }
#endif
  return;

unhandled:
  fprintf(stderr, "unhandled IO op %08x @ %08x\n", op, pc);
}

#define LINKPAGE_SIZE 0x1000
#define LINKPAGE_COUNT 4
#define LINKPAGE_ALLOC (LINKPAGE_SIZE * LINKPAGE_COUNT)

struct linkpage {
  u32 saved_regs[15];
  u32 *lp_r1;
  void (*handler)(u32 addr_pc, u32 op, u32 *regs, u32 addr_check);
  u32 code[0];
};

static struct linkpage *g_linkpage;
static u32 *g_code_ptr;
static int g_linkpage_count;

static void init_linkpage(void)
{
  g_linkpage->lp_r1 = &g_linkpage->saved_regs[1];
  g_linkpage->handler = handle_op;
  g_code_ptr = g_linkpage->code;
}

static u32 make_offset12(u32 *pc, u32 *target)
{
  int lp_offs, u = 1;

  lp_offs = (char *)target - (char *)pc - 2*4;
  if (lp_offs < 0) {
    lp_offs = -lp_offs;
    u = 0;
  }
  if (lp_offs >= LINKPAGE_SIZE) {
    fprintf(stderr, "linkpage too far: %d\n", lp_offs);
    abort();
  }

  return (u << 23) | lp_offs;
}

static u32 make_jmp(u32 *pc, u32 *target)
{
  int jmp_val;

  jmp_val = target - pc - 2;
  if (jmp_val < (int)0xff000000 || jmp_val > 0x00ffffff) {
    fprintf(stderr, "jump out of range (%p -> %p)\n", pc, target);
    abort();
  }

  return 0xea000000 | (jmp_val & 0x00ffffff);
}

static void emit_op(u32 op)
{
  *g_code_ptr++ = op;
}

static void emit_op_io(u32 op, u32 *target)
{
  op |= make_offset12(g_code_ptr, target);
  emit_op(op);
}

static void segv_sigaction(int num, siginfo_t *info, void *ctx)
{
  struct ucontext *context = ctx;
  u32 *regs = (u32 *)&context->uc_mcontext.arm_r0;
  u32 *pc = (u32 *)regs[15];
  u32 old_op = *pc;
  u32 *pc_ptr, *old_op_ptr;
  int lp_size;

  if (((regs[15] ^ (u32)&segv_sigaction) & 0xff000000) == 0 ||       // PC is in our segment or
      (((regs[15] ^ (u32)g_linkpage) & ~(LINKPAGE_ALLOC - 1)) == 0)) // .. in linkpage
  {
    // real crash - time to die
    printf("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);
    signal(num, SIG_DFL);
    raise(num);
  }
  printf("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);

  // spit PC and op
  pc_ptr = g_code_ptr++;
  old_op_ptr = g_code_ptr++;
  *pc_ptr = (u32)pc;
  *old_op_ptr = old_op;

  // emit jump to code ptr
  *pc = make_jmp(pc, g_code_ptr);

  // generate code:
  // TODO: our own stack
  emit_op_io(0xe50f0000, &g_linkpage->saved_regs[0]);  // str r0, [saved_regs[0]] @ save r0
  emit_op_io(0xe51f0000, (u32 *)&g_linkpage->lp_r1);   // ldr r0, =lp_r1
  emit_op   (0xe8807ffe);                              // stmia r0, {r1-r14}
  emit_op   (0xe2402004);                              // sub r2, r0, #4
  emit_op_io(0xe51f0000, pc_ptr);                      // ldr r0, =pc
  emit_op_io(0xe51f1000, old_op_ptr);                  // ldr r1, =old_op
  emit_op   (0xe1a04002);                              // mov r4, r2
  emit_op   (0xe1a0e00f);                              // mov lr, pc
  emit_op_io(0xe51ff000, (u32 *)&g_linkpage->handler); // ldr pc, =handle_op
  emit_op   (0xe8947fff);                              // ldmia r4, {r0-r14}
  emit_op   (make_jmp(g_code_ptr, pc + 1));            // jmp <back>

  // sync caches
  sys_cacheflush(pc, pc + 1);
  sys_cacheflush(g_linkpage, g_code_ptr);

  lp_size = (char *)g_code_ptr - (char *)g_linkpage;
  printf("code #%d %d/%d\n", g_linkpage_count, lp_size, LINKPAGE_SIZE);

  if (lp_size + 13*4 > LINKPAGE_SIZE) {
    g_linkpage_count++;
    if (g_linkpage_count >= LINKPAGE_COUNT) {
      fprintf(stderr, "too many linkpages needed\n");
      abort();
    }
    g_linkpage = (void *)((char *)g_linkpage + LINKPAGE_SIZE);
    init_linkpage();
  }
  //handle_op(regs[15], op, regs, (u32)info->si_addr);
  //regs[15] += 4;
}

void emu_init(void *map_bottom)
{
  struct sigaction segv_action = {
    .sa_sigaction = segv_sigaction,
    .sa_flags = SA_SIGINFO,
  };
  void *ret;

  sigemptyset(&segv_action.sa_mask);
  sigaction(SIGSEGV, &segv_action, NULL);

  g_linkpage = (void *)(((u32)map_bottom - LINKPAGE_ALLOC) & ~0xfff);
  ret = mmap(g_linkpage, LINKPAGE_ALLOC, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (ret != g_linkpage) {
    perror("mmap linkpage");
    exit(1);
  }
  printf("linkpages @ %p\n", g_linkpage);
  init_linkpage();

  memdev = open("/dev/mem", O_RDWR);
  memregs = mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
  blitter = mmap(NULL, 0x100, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xe0020000);
  //blitter = mmap(NULL, 0x100, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
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

