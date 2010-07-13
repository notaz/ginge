// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
//#define segvlog printf
#define segvlog(...)

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

struct uppermem_block {
  u32 addr; // physical
  u32 size;
  void *mem;
  struct uppermem_block *next;
};

static struct uppermem_block *upper_mem;

static struct {
  u32 dstctrl;
  u32 dstaddr;
  u32 dststride;
  u32 srcctrl;
  u32 srcaddr;          //
  u32 srcstride;
  u32 srcforcolor;
  u32 srcbackcolor;
  u32 patctrl;          //
  u32 patforcolor;
  u32 patbackcolor;
  u32 size;
  u32 ctrl;             //
  u32 run;
  u32 intc;
  u32 srcfifo;
} blitter;

#define SRCCTRL_INVIDEO         (1 << 8)
#define SRCCTRL_SRCENB          (1 << 7)
#define CTRL_TRANSPARENCYENB    (1 << 11)

static struct {
  union {
    u32 mlc_stl_eadr;
    struct {
      u16 mlc_stl_eadrl;
      u16 mlc_stl_eadrh;
    };
  };
} mmsp2;

static u16 *host_screen;
static int host_stride;


static void memset16(void *dst, u32 pattern, int count)
{
  u32 *dl;
  u16 *d;
  
  d = (u16 *)((long)dst & ~1);
  if ((long)d & 2) {
    *d++ = pattern;
    count--;
  }
  dl = (void *)d;
  pattern |= pattern << 16;

  while (count >= 2) {
    *dl++ = pattern;
    count -= 2;
  }
  if (count)
    *(u16 *)dl = pattern;
}

static void blt_tr(void *dst, void *src, u32 trc, int w)
{
  u16 *d = (u16 *)((long)dst & ~1);
  u16 *s = (u16 *)((long)src & ~1);

  // XXX: optimize
  for (; w > 0; d++, s++, w--)
    if (*s != trc)
      *d = *s;
}

#define dump_blitter() \
{ \
  u32 *r = &blitter.dstctrl; \
  int i; \
  for (i = 0; i < 4*4; i++, r++) { \
    printf("%08x ", *r); \
    if ((i & 3) == 3) \
      printf("\n"); \
  } \
}

static void *upper_lookup(u32 addr, u8 **mem_end, int *stride_override)
{
  struct uppermem_block *ub;

  // maybe the screen?
  if (mmsp2.mlc_stl_eadr <= addr && addr < mmsp2.mlc_stl_eadr + 320*240*2) {
    host_screen = host_video_flip(); // HACK
    *mem_end = (u8 *)host_screen + host_stride * 240;
    *stride_override = host_stride;
    return (u8 *)host_screen + addr - mmsp2.mlc_stl_eadr;
  }

  for (ub = upper_mem; ub != NULL; ub = ub->next) {
    if (ub->addr <= addr && addr < ub->addr + ub->size) {
      *mem_end = (u8 *)ub->mem + ub->size;
      return (u8 *)ub->mem + addr - ub->addr;
    }
  }

  return NULL;
}

static void blitter_do(void)
{
  u8 *dst, *dste, *src = NULL, *srce = NULL;
  int w, h, sstrd, dstrd;
  u32 addr;

  w = blitter.size & 0x7ff;
  h = (blitter.size >> 16) & 0x7ff;
  sstrd = blitter.srcstride;
  dstrd = blitter.dststride;

  // XXX: need to confirm this..
  addr = (blitter.dstaddr & ~3) | ((blitter.dstctrl & 0x1f) >> 3);
  dst = upper_lookup(addr, &dste, &dstrd);
  if (dst == NULL)
    goto bad_blit;

  // XXX: assume fill if no SRCENB, but it could be pattern blit..
  if (blitter.srcctrl & SRCCTRL_SRCENB) {
    if (!(blitter.srcctrl & SRCCTRL_INVIDEO))
      goto bad_blit;

    addr = (blitter.srcaddr & ~3) | ((blitter.srcctrl & 0x1f) >> 3);
    src = upper_lookup(addr, &srce, &sstrd);
    if (src == NULL)
      goto bad_blit;

    if (src + sstrd * h > srce) {
      printf("blit %08x->%08x %dx%d did not fit src\n",
        blitter.srcaddr, blitter.dstaddr, w, h);
      h = (srce - src) / sstrd;
    }
  }

  if (dst + dstrd * h > dste) {
    printf("blit %08x->%08x %dx%d did not fit dst\n",
      blitter.srcaddr, blitter.dstaddr, w, h);
    h = (dste - dst) / dstrd;
  }

  if (src != NULL) {
    // copy
    if (blitter.ctrl & CTRL_TRANSPARENCYENB) {
      u32 trc = blitter.ctrl >> 16;
      for (; h > 0; h--, dst += dstrd, src += sstrd)
        blt_tr(dst, src, trc, w);
    }
    else {
      for (; h > 0; h--, dst += dstrd, src += sstrd)
        memcpy(dst, src, w * 2);
    }
  }
  else {
    // fill. Assume the pattern is cleared and bg color is used
    u32 bgc = blitter.patbackcolor & 0xffff;
    for (; h > 0; h--, dst += dstrd)
      memset16(dst, bgc, w);
  }
  return;

bad_blit:
  printf("blit %08x->%08x %dx%d translated to %p->%p\n",
    blitter.srcaddr, blitter.dstaddr, w, h, src, dst);
  dump_blitter();
}

static u32 xread8(u32 a)
{
  iolog("r8  %08x\n", a);
  return 0;
}

static u32 xread16(u32 a)
{
// if ((a & 0xfff00000) == 0x7f100000) { static int a; a ^= ~1; return a & 0xffff; }
  iolog("r16 %08x\n", a);
  return 0;
}

static u32 xread32(u32 a)
{
  u32 d = 0;
  if ((a & 0xfff00000) == 0x7f100000) {
    u32 *bl = &blitter.dstctrl;
    a &= 0xfff;
    if (a < 0x40)
      d = bl[a / 4];
    if (a == 0x34)
      d = 0; // not busy
  }
  iolog("r32 %08x\n", a);
  return d;
}

static void xwrite8(u32 a, u32 d)
{
  iolog("w8  %08x %08x\n", a, d);
}

static void xwrite16(u32 a, u32 d)
{
  iolog("w16 %08x %08x\n", a, d);
  if ((a & 0xfff00000) == 0x7f000000) {
    a &= 0xffff;
    switch (a) {
      case 0x2912: mmsp2.mlc_stl_eadrl = d; break;
      case 0x2914: mmsp2.mlc_stl_eadrh = d; break;
    }
    //printf("w16 %08x %08x\n", a, d);
  }
}

static void xwrite32(u32 a, u32 d)
{
  iolog("w32 %08x %08x\n", a, d);
  if ((a & 0xfff00000) == 0x7f000000) {
    printf("w32 %08x %08x\n", a, d);
    return;
  }
  if ((a & 0xfff00000) == 0x7f100000) {
    u32 *bl = &blitter.dstctrl;
    a &= 0xfff;
    if (a < 0x40)
      bl[a / 4] = d;
    if (a == 0x34 && (d & 1))
      blitter_do();
    return;
  }
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
  segvlog("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);

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
  segvlog("code #%d %d/%d\n", g_linkpage_count, lp_size, LINKPAGE_SIZE);

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

  // host stuff
  ret = host_video_init(&host_stride, 0);
  if (ret != 0) {
    printf("can't alloc screen\n");
    exit(1);
  }
  host_screen = host_video_flip();
}

int emu_read_gpiodev(void *buf, int count)
{
  unsigned int btns;

  if (count < 4) {
    printf("gpiodev read %d?\n", count);
    return -1;
  }

  btns = host_read_btns();
  memcpy(buf, &btns, 4);
  return 4;
}

void *emu_mmap_dev(unsigned int length, int prot, int flags, unsigned int offset)
{
  struct uppermem_block *umem;
  char name[32];
  int fd;

  // SoC regs
  if ((offset & ~0xffff) == 0xc0000000) {
    return mmap((void *)0x7f000000, length, PROT_NONE,
      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_NORESERVE, -1, 0);
  }
  // blitter
  if ((offset & ~0xffff) == 0xe0020000) {
    return mmap((void *)0x7f100000, length, PROT_NONE,
      MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED|MAP_NORESERVE, -1, 0);
  }
  // upper mem
  if ((offset & 0xfe000000) != 0x02000000)
    printf("unexpected devmem mmap @ %08x\n", offset);

  // return mmap(NULL, length, prot, flags, memdev, offset);

  umem = calloc(1, sizeof(*umem));
  if (umem == NULL) {
    printf("OOM\n");
    return MAP_FAILED;
  }

  umem->addr = offset;
  umem->size = length;
  umem->mem = mmap(NULL, length, prot, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (umem->mem != MAP_FAILED)
    goto done;

  printf("upper mem @ %08x %d mmap fail, trying backing file\n", offset, length);
  sprintf(name, "m%08x", offset);
  fd = open(name, O_CREAT|O_RDWR, 0644);
  lseek(fd, length - 1, SEEK_SET);
  name[0] = 0;
  write(fd, name, 1);

  umem->mem = mmap(NULL, length, prot, MAP_SHARED, fd, 0);
  if (umem->mem == MAP_FAILED) {
    printf("failed, giving up\n");
    close(fd);
    free(umem);
    return MAP_FAILED;
  }

done:
  printf("upper mem @ %08x %d\n", offset, length);
  umem->next = upper_mem;
  upper_mem = umem;
  return umem->mem;
}

