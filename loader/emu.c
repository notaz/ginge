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

//#define LOG_IO
//#define LOG_IO_UNH
//#define LOG_SEGV

#ifdef LOG_IO
#define iolog log_io
#else
#define iolog(...)
#endif

#ifdef LOG_IO_UNH
#define iolog_unh log_io
#else
#define iolog_unh(...)
#endif

#ifdef LOG_SEGV
#define segvlog printf
#else
#define segvlog(...)
#endif

#if defined(LOG_IO) || defined(LOG_IO_UNH)
#include "mmsp2-regs.h"
#endif

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
  u16 mlc_stl_cntl;
  union {
    u32 mlc_stl_adr;
    struct {
      u16 mlc_stl_adrl;
      u16 mlc_stl_adrh;
    };
  };
  u16 mlc_stl_pallt_a;
  union {
    u16 mlc_stl_pallt_d[256*2];
    u32 mlc_stl_pallt_d32[256];
  };

  // state
  u16 host_pal[256];
  u32 old_mlc_stl_adr;
  u32 btn_state; // as seen through /dev/GPIO
  u16 dirty_pal:1;
} mmsp2;

static u16 *host_screen;
static int host_stride;


#if defined(LOG_IO) || defined(LOG_IO_UNH)
static void log_io(const char *pfx, u32 a, u32 d, int size)
{
  const char *fmt, *reg = "";
  switch (size) {
  case  8: fmt = "%s %08x       %02x %s\n"; break;
  case 32: fmt = "%s %08x %08x %s\n"; break;
  default: fmt = "%s %08x     %04x %s\n"; break;
  }

  if ((a & ~0xffff) == 0x7f000000)
    reg = regnames[a & 0xffff];

  printf(fmt, pfx, a, d, reg);
}
#endif

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

static void *uppermem_lookup(u32 addr, u8 **mem_end)
{
  struct uppermem_block *ub;

  for (ub = upper_mem; ub != NULL; ub = ub->next) {
    if (ub->addr <= addr && addr < ub->addr + ub->size) {
      *mem_end = (u8 *)ub->mem + ub->size;
      return (u8 *)ub->mem + addr - ub->addr;
    }
  }

  return NULL;
}

static void *blitter_mem_lookup(u32 addr, u8 **mem_end, int *stride_override, int *to_screen)
{
  // maybe the screen?
  if (mmsp2.mlc_stl_adr <= addr && addr < mmsp2.mlc_stl_adr + 320*240*2) {
    *mem_end = (u8 *)host_screen + host_stride * 240;
    *stride_override = host_stride;
    *to_screen = 1;
    return (u8 *)host_screen + addr - mmsp2.mlc_stl_adr;
  }

  return uppermem_lookup(addr, mem_end);
}

static void blitter_do(void)
{
  u8 *dst, *dste, *src = NULL, *srce = NULL;
  int w, h, sstrd, dstrd;
  int to_screen = 0;
  u32 addr;

  w = blitter.size & 0x7ff;
  h = (blitter.size >> 16) & 0x7ff;
  sstrd = blitter.srcstride;
  dstrd = blitter.dststride;

  // XXX: need to confirm this..
  addr = (blitter.dstaddr & ~3) | ((blitter.dstctrl & 0x1f) >> 3);
  dst = blitter_mem_lookup(addr, &dste, &dstrd, &to_screen);
  if (dst == NULL)
    goto bad_blit;

  // XXX: assume fill if no SRCENB, but it could be pattern blit..
  if (blitter.srcctrl & SRCCTRL_SRCENB) {
    if (!(blitter.srcctrl & SRCCTRL_INVIDEO))
      goto bad_blit;

    addr = (blitter.srcaddr & ~3) | ((blitter.srcctrl & 0x1f) >> 3);
    src = blitter_mem_lookup(addr, &srce, &sstrd, &to_screen);
    if (src == NULL)
      goto bad_blit;

    if (src + sstrd * h > srce) {
      err("blit %08x->%08x %dx%d did not fit src\n",
        blitter.srcaddr, blitter.dstaddr, w, h);
      h = (srce - src) / sstrd;
    }
  }

  if (dst + dstrd * h > dste) {
    err("blit %08x->%08x %dx%d did not fit dst\n",
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

  if (to_screen)
    host_screen = host_video_flip();
  return;

bad_blit:
  err("blit %08x->%08x %dx%d translated to %p->%p\n",
    blitter.srcaddr, blitter.dstaddr, w, h, src, dst);
  dump_blitter();
}

// TODO: hw scaler stuff
static void mlc_flip(u32 addr)
{
  int mode = (mmsp2.mlc_stl_cntl >> 9) & 3;
  int bpp = mode ? mode * 8 : 4;
  u16 *dst = host_screen;
  u16 *hpal = mmsp2.host_pal;
  u8 *src, *src_end;
  int i, u;

  src = uppermem_lookup(addr, &src_end);
  if (src == NULL || src + 320*240 * bpp / 8 > src_end) {
    err("mlc_flip: %08x is out of range\n", addr);
    return;
  }

  if (bpp <= 8 && mmsp2.dirty_pal) {
    u32 *srcp = mmsp2.mlc_stl_pallt_d32;
    u16 *dstp = hpal;

    for (i = 0; i < 256; i++, srcp++, dstp++) {
      u32 t = *srcp;
      *dstp = ((t >> 8) & 0xf800) | ((t >> 5) & 0x07e0) | ((t >> 3) & 0x001f);
    }
    mmsp2.dirty_pal = 0;
  }

  switch (bpp) {
  case  4:
    for (i = 0; i < 240; i++, dst += host_stride / 2 - 320) {
      for (u = 320 / 2; u > 0; u--, src++) {
        *dst++ = hpal[*src >> 4];
        *dst++ = hpal[*src & 0x0f];
      }
    }
    break;

  case  8:
    for (i = 0; i < 240; i++, dst += host_stride / 2 - 320) {
      for (u = 320 / 4; u > 0; u--) {
        *dst++ = hpal[*src++];
        *dst++ = hpal[*src++];
        *dst++ = hpal[*src++];
        *dst++ = hpal[*src++];
      }
    }
    break;

  case 16:
    for (i = 0; i < 240; i++, dst += host_stride / 2, src += 320*2)
      memcpy(dst, src, 320*2);
    break;

  case 24:
    // TODO
    break;
  }

  host_screen = host_video_flip();
}

static u32 xread8(u32 a)
{
  iolog("r8 ", a, 0, 8);
  iolog_unh("r8 ", a, 0, 8);
  return 0;
}

static u32 xread16(u32 a)
{
  static u32 fudge, old_a;
  u32 d = 0, t;

  if ((a & 0xffff0000) == 0x7f000000) {
    u32 a_ = a & 0xffff;
    switch (a_) {
    case 0x0910: // FPLL
    case 0x0912:
      d = 0x9407;
      break;
    // minilib reads as:
    //  0000 P000 VuVd00 0000 YXBA RLSeSt 0R0D 0L0U
    // |        GPIOD        |GPIOC[8:15]|GPIOM[0:7]|
    // /dev/GPIO:
    //             ... 0PVdVu ...
    case 0x1184: // GPIOC
      d = ~mmsp2.btn_state & 0xff00;
      d |= 0x00ff;
      break;
    case 0x1186: // GPIOD
      t = ~mmsp2.btn_state;
      d  = (t >> 9)  & 0x0080;
      d |= (t >> 11) & 0x0040;
      d |= (t >> 7)  & 0x0800;
      d |= 0x373b;
      break;
    case 0x1198: // GPIOM
      mmsp2.btn_state = host_read_btns();
      d = ~mmsp2.btn_state & 0xff;
      d |= 0x01aa;
      break;
    case 0x28da:
      d = mmsp2.mlc_stl_cntl;
      break;
    case 0x2958:
      d = mmsp2.mlc_stl_pallt_a;
      break;
    default:
      goto unh;
    }
    goto out;
  }

unh:
  if (a == old_a) {
    d = fudge;
    fudge = ~fudge;
  }
  old_a = a;
  iolog_unh("r16", a, d & 0xffff, 16);

out:
  d &= 0xffff;
  iolog("r16", a, d, 16);
  return d;
}

static u32 xread32(u32 a)
{
  u32 d = 0;
  if ((a & 0xfff00000) == 0x7f100000) {
    u32 *bl = &blitter.dstctrl;
    u32 a_ = a & 0xfff;
    if (a_ < 0x40) {
      d = bl[a_ / 4];
      if (a_ == 0x34)
        d = 0; // not busy
      goto out;
    }
  }
  iolog_unh("r32", a, d, 32);

out:
  iolog("r32", a, d, 32);
  return d;
}

static void xwrite8(u32 a, u32 d)
{
  iolog("w8 ", a, d, 8);
  iolog_unh("w8 ", a, d, 8);
}

static void xwrite16(u32 a, u32 d)
{
  iolog("w16", a, d, 16);
  if ((a & 0xfff00000) == 0x7f000000) {
    u32 a_ = a & 0xffff;
    switch (a_) {
    case 0x28da:
      mmsp2.mlc_stl_cntl = d | 0xaa;
      break;
    case 0x290e:
    case 0x2910:
      // odd addresses don't affect LCD. What about TV?
      return;
    case 0x2912:
      mmsp2.mlc_stl_adrl = d;
      return;
    case 0x2914:
      mmsp2.mlc_stl_adrh = d;
      if (mmsp2.mlc_stl_adr != mmsp2.old_mlc_stl_adr)
        mlc_flip(mmsp2.mlc_stl_adr);
      mmsp2.old_mlc_stl_adr = mmsp2.mlc_stl_adr;
      return;
    case 0x2958:
      mmsp2.mlc_stl_pallt_a = d & 0x1ff;
      return;
    case 0x295a:
      mmsp2.mlc_stl_pallt_d[mmsp2.mlc_stl_pallt_a++] = d;
      mmsp2.mlc_stl_pallt_a &= 0x1ff;
      mmsp2.dirty_pal = 1;
      return;
    }
  }
  iolog_unh("w16", a, d, 16);
}

static void xwrite32(u32 a, u32 d)
{
  iolog("w32", a, d, 32);

  if ((a & 0xfff00000) == 0x7f100000) {
    u32 *bl = &blitter.dstctrl;
    u32 a_ = a & 0xfff;
    if (a_ < 0x40) {
      bl[a_ / 4] = d;
      if (a_ == 0x34 && (d & 1))
        blitter_do();
      return;
    }
  }
  iolog_unh("w32", a, d, 32);
}

#define LINKPAGE_SIZE 0x1000
#define LINKPAGE_COUNT 4
#define LINKPAGE_ALLOC (LINKPAGE_SIZE * LINKPAGE_COUNT)

struct op_context {
  u32 pc;
  u32 op;
  u32 code[0];
};

struct linkpage {
  u32 saved_regs[15];
  u32 cpsr;
  u32 *handler_stack;
  void (*handler)(struct op_context *op_ctx);
  u32 code[0];
};

static struct linkpage *g_linkpage;
static u32 *g_code_ptr;
static int g_linkpage_count;

#define HANDLER_STACK_SIZE 4096
static void *g_handler_stack_end;

#define BIT_SET(v, b) (v & (1 << (b)))

static void handle_op(struct op_context *op_ctx)
{
  u32 *regs = g_linkpage->saved_regs;
  u32 op = op_ctx->op;
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
  err("unhandled IO op %08x @ %08x\n", op, op_ctx->pc);
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
    err("linkpage too far: %d\n", lp_offs);
    abort();
  }

  return (u << 23) | lp_offs;
}

static u32 make_jmp(u32 *pc, u32 *target, int bl)
{
  int jmp_val;

  jmp_val = target - pc - 2;
  if (jmp_val < (int)0xff000000 || jmp_val > 0x00ffffff) {
    err("jump out of range (%p -> %p)\n", pc, target);
    abort();
  }

  return 0xea000000 | (bl << 24) | (jmp_val & 0x00ffffff);
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

static void init_linkpage(void)
{
  g_linkpage->handler = handle_op;
  g_linkpage->handler_stack = g_handler_stack_end;
  g_code_ptr = g_linkpage->code;

  // common_code.
  // r0 and r14 must be saved by caller, r0 is arg for handle_op
  // on return everything is restored except lr, which is used to return
  emit_op_io(0xe50f1000, &g_linkpage->saved_regs[1]);  // str r1, [->saved_regs[1]] @ save r1
  emit_op   (0xe24f1000 +                              // sub r1, pc, =offs(saved_regs[2])
    (g_code_ptr - &g_linkpage->saved_regs[2] + 2) * 4);
  emit_op   (0xe8813ffc);                              // stmia r1, {r2-r13}
  emit_op_io(0xe51fd000,                               // ldr sp, [->handler_stack]
    (u32 *)&g_linkpage->handler_stack);
  emit_op   (0xe2414008);                              // sub r4, r1, #4*2
  emit_op   (0xe10f1000);                              // mrs r1, cpsr
  emit_op_io(0xe50f1000, &g_linkpage->cpsr);           // str r1, [->cpsr]
  emit_op   (0xe1a0500e);                              // mov r5, lr
  emit_op   (0xe1a0e00f);                              // mov lr, pc
  emit_op_io(0xe51ff000, (u32 *)&g_linkpage->handler); // ldr pc, =handle_op
  emit_op_io(0xe51f1000, &g_linkpage->cpsr);           // ldr r1, [->cpsr]
  emit_op   (0xe128f001);                              // msr cpsr_f, r1
  emit_op   (0xe1a0e005);                              // mov lr, r5
  emit_op   (0xe8943fff);                              // ldmia r4, {r0-r13}
  emit_op   (0xe12fff1e);                              // bx lr @ return
}

static void segv_sigaction(int num, siginfo_t *info, void *ctx)
{
  struct ucontext *context = ctx;
  u32 *regs = (u32 *)&context->uc_mcontext.arm_r0;
  u32 *pc = (u32 *)regs[15];
  struct op_context *op_ctx;
  int lp_size;

  if (((regs[15] ^ (u32)&segv_sigaction) & 0xff000000) == 0 ||         // PC is in our segment or
      (((regs[15] ^ (u32)g_linkpage) & ~(LINKPAGE_ALLOC - 1)) == 0) || // .. in linkpage
      ((long)info->si_addr & 0xffe00000) != 0x7f000000)                // faulting not where expected
  {
    // real crash - time to die
    err("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);
    signal(num, SIG_DFL);
    raise(num);
  }
  segvlog("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);

  // spit PC and op
  op_ctx = (void *)g_code_ptr;
  op_ctx->pc = (u32)pc;
  op_ctx->op = *pc;
  g_code_ptr = &op_ctx->code[0];

  // emit jump to code ptr
  *pc = make_jmp(pc, g_code_ptr, 0);

  // generate code:
  // TODO: multithreading
  emit_op_io(0xe50f0000, &g_linkpage->saved_regs[0]);            // str r0,  [->saved_regs[0]] @ save r0
  emit_op_io(0xe50fe000, &g_linkpage->saved_regs[14]);           // str r14, [->saved_regs[14]]
  emit_op   (0xe24f0000 + (g_code_ptr - (u32 *)op_ctx + 2) * 4); // sub r0, pc, #op_ctx
  emit_op   (make_jmp(g_code_ptr, &g_linkpage->code[0], 1));     // bl common_code
  emit_op_io(0xe51fe000, &g_linkpage->saved_regs[14]);           // ldr r14, [->saved_regs[14]]
  emit_op   (make_jmp(g_code_ptr, pc + 1, 0));                   // jmp <back>

  // sync caches
  sys_cacheflush(pc, pc + 1);
  sys_cacheflush(g_linkpage, g_code_ptr);

  lp_size = (char *)g_code_ptr - (char *)g_linkpage;
  segvlog("code #%d %d/%d\n", g_linkpage_count, lp_size, LINKPAGE_SIZE);

  if (lp_size + 13*4 > LINKPAGE_SIZE) {
    g_linkpage_count++;
    if (g_linkpage_count >= LINKPAGE_COUNT) {
      err("too many linkpages needed\n");
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
  void *pret;
  int ret;

  sigemptyset(&segv_action.sa_mask);
  sigaction(SIGSEGV, &segv_action, NULL);

  pret = mmap(NULL, HANDLER_STACK_SIZE + 4096, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
  if (pret == MAP_FAILED) {
    perror(PFX "mmap handler_stack");
    exit(1);
  }
  ret = mprotect((char *)pret + 4096, HANDLER_STACK_SIZE, PROT_READ | PROT_WRITE);
  if (ret != 0) {
    perror(PFX "mprotect handler_stack");
    exit(1);
  }
  g_handler_stack_end = (char *)pret + HANDLER_STACK_SIZE + 4096;

  g_linkpage = (void *)(((u32)map_bottom - LINKPAGE_ALLOC) & ~0xfff);
  pret = mmap(g_linkpage, LINKPAGE_ALLOC, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (pret != g_linkpage) {
    perror(PFX "mmap linkpage");
    exit(1);
  }
  printf("linkpages @ %p\n", g_linkpage);
  init_linkpage();

  // host stuff
  ret = host_video_init(&host_stride, 0);
  if (ret != 0) {
    err("can't alloc screen\n");
    exit(1);
  }
  host_screen = host_video_flip();
}

int emu_read_gpiodev(void *buf, int count)
{
  unsigned int btns;

  if (count < 4) {
    err("gpiodev read %d?\n", count);
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
    err("unexpected devmem mmap @ %08x\n", offset);

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
    err("failed, giving up\n");
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

