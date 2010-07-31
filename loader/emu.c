// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <asm/ucontext.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <linux/fb.h>

#include "header.h"
#include "../common/host_fb.h"
#include "../common/cmn.h"
#include "sys_cacheflush.h"
#include "realfuncs.h"

#if (DBG & 2) && !(DBG & 4)
#define LOG_IO_UNK
#endif
#if (DBG & 4)
#define LOG_IO
#endif
//#define LOG_SEGV

#ifdef LOG_IO
#define iolog log_io
#else
#define iolog(...)
#endif

#ifdef LOG_IO_UNK
#define iolog_unh log_io
#else
#define iolog_unh(...)
#endif

#ifdef LOG_SEGV
#define segvlog printf
#else
#define segvlog(...)
#endif

#if defined(LOG_IO) || defined(LOG_IO_UNK)
#include "mmsp2-regs.h"
#endif

typedef unsigned long long u64;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

static pthread_mutex_t fb_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fb_cond = PTHREAD_COND_INITIALIZER;

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
  void *umem;
  u32 old_mlc_stl_adr;
  u32 btn_state; // as seen through /dev/GPIO
  u32 dirty_pal:1;
} mmsp2;


#if defined(LOG_IO) || defined(LOG_IO_UNK)
static void log_io(const char *pfx, u32 a, u32 d, int size)
{
  const char *fmt, *reg = "";
  switch (size) {
  case  8: fmt = "%s %08x       %02x %s\n"; d &= 0xff; break;
  case 32: fmt = "%s %08x %08x %s\n";       break;
  default: fmt = "%s %08x     %04x %s\n";   d &= 0xffff; break;
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
  // XXX: maybe support mirroring?
  if ((addr & 0xfe000000) != 0x02000000)
    return NULL;

  *mem_end = (u8 *)mmsp2.umem + 0x02000000;
  return (u8 *)mmsp2.umem - 0x02000000 + addr;
}

static void blitter_do(void)
{
  u8 *dst, *dste, *src = NULL, *srce = NULL;
  int w, h, sstrd, dstrd;
  int to_screen = 0;
  u32 bpp, addr;

  w = blitter.size & 0x7ff;
  h = (blitter.size >> 16) & 0x7ff;
  sstrd = blitter.srcstride;
  dstrd = blitter.dststride;

  // XXX: need to confirm this..
  addr = (blitter.dstaddr & ~3) | ((blitter.dstctrl & 0x1f) >> 3);

  // use dst bpp.. How does it do blits with different src bpp?
  bpp = (blitter.dstctrl & 0x20) ? 16 : 8;

  // maybe the screen?
  if (((w == 320 && h == 240) || // blit whole screen
       (w * h >= 320*240/2)) &&  // ..or at least half of the area
       mmsp2.mlc_stl_adr <= addr && addr < mmsp2.mlc_stl_adr + 320*240*2)
    to_screen = 1;

  dst = uppermem_lookup(addr, &dste);

  // XXX: assume fill if no SRCENB, but it could be pattern blit..
  if (blitter.srcctrl & SRCCTRL_SRCENB) {
    if (!(blitter.srcctrl & SRCCTRL_INVIDEO))
      goto bad_blit;

    addr = (blitter.srcaddr & ~3) | ((blitter.srcctrl & 0x1f) >> 3);
    src = uppermem_lookup(addr, &srce);
    if (src == NULL)
      goto bad_blit;

    if (src + sstrd * h > srce) {
      err("blit %08x->%08x %dx%d did not fit src\n",
        blitter.srcaddr, blitter.dstaddr, w, h);
      h = (srce - src) / sstrd;
    }
  }

  if (dst == NULL)
    goto bad_blit;

  if (dst + dstrd * h > dste) {
    err("blit %08x->%08x %dx%d did not fit dst\n",
      blitter.srcaddr, blitter.dstaddr, w, h);
    h = (dste - dst) / dstrd;
  }

  if (src != NULL) {
    // copy
    if (bpp == 16 && (blitter.ctrl & CTRL_TRANSPARENCYENB)) {
      u32 trc = blitter.ctrl >> 16;
      for (; h > 0; h--, dst += dstrd, src += sstrd)
        blt_tr(dst, src, trc, w);
    }
    else {
      for (; h > 0; h--, dst += dstrd, src += sstrd)
        memcpy(dst, src, w * bpp / 8);
    }
  }
  else {
    // fill. Assume the pattern is cleared and bg color is used
    u32 bgc = blitter.patbackcolor & 0xffff;
    if (bpp == 16) {
      for (; h > 0; h--, dst += dstrd)
        memset16(dst, bgc, w);
    }
    else {
      for (; h > 0; h--, dst += dstrd)
        memset(dst, bgc, w); // bgc?
    }
  }

  if (to_screen)
    pthread_cond_signal(&fb_cond);
  return;

bad_blit:
  err("blit %08x->%08x %dx%d translated to %p->%p\n",
    blitter.srcaddr, blitter.dstaddr, w, h, src, dst);
  dump_blitter();
}

// FIXME: pass real dimensions to blitters
static void mlc_flip(void *src, int bpp)
{
  static int old_bpp;

  // only pass pal to host if it's dirty
  if (bpp <= 8 && mmsp2.dirty_pal) {
    host_video_update_pal(mmsp2.mlc_stl_pallt_d32);
    mmsp2.dirty_pal = 0;
  }

  if (bpp != old_bpp) {
    host_video_change_bpp(bpp);
    old_bpp = bpp;
  }

  switch (bpp) {
  case  4:
    host_video_blit4(src, 320, 240);
    break;

  case  8:
    host_video_blit8(src, 320, 240);
    break;

  case 16:
    host_video_blit16(src, 320, 240);
    break;

  case 24:
    // TODO
    break;
  }
}

#define ts_add_nsec(ts, ns) { \
  ts.tv_nsec += ns; \
  if (ts.tv_nsec >= 1000000000) { \
    ts.tv_sec++; \
    ts.tv_nsec -= 1000000000; \
  } \
}

static void *fb_sync_thread(void *arg)
{
  int invalid_fb_addr = 1;
  int manual_refresh = 0;
  struct timespec ts;
  int ret, wait_ret;

  //ret = pthread_setschedprio(pthread_self(), -1);
  //log("pthread_setschedprio %d\n", ret);
  //ret = setpriority(PRIO_PROCESS, 0, -1);
  //log("setpriority %d\n", ret);

  ret = clock_gettime(CLOCK_REALTIME, &ts);
  if (ret != 0) {
    perror(PFX "clock_gettime");
    exit(1);
  }

  while (1) {
    u8 *gp2x_fb, *gp2x_fb_end;
    int mode, bpp;

    ret =  pthread_mutex_lock(&fb_mutex);
    wait_ret = pthread_cond_timedwait(&fb_cond, &fb_mutex, &ts);
    ret |= pthread_mutex_unlock(&fb_mutex);

    if (ret != 0) {
      err("fb_thread: mutex error: %d\n", ret);
      sleep(1);
      continue;
    }
    if (wait_ret != 0 && wait_ret != ETIMEDOUT) {
      err("fb_thread: cond error: %d\n", wait_ret);
      sleep(1);
      continue;
    }

    if (wait_ret != ETIMEDOUT) {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts_add_nsec(ts, 50000000);
      manual_refresh++;
      if (manual_refresh == 2)
        dbg("fb_thread: switch to manual refresh\n");
    } else {
      ts_add_nsec(ts, 16666667);
      if (manual_refresh > 1)
        dbg("fb_thread: switch to auto refresh\n");
      manual_refresh = 0;
    }

    mode = (mmsp2.mlc_stl_cntl >> 9) & 3;
    bpp = mode ? mode * 8 : 4;

    gp2x_fb = uppermem_lookup(mmsp2.mlc_stl_adr, &gp2x_fb_end);
    if (gp2x_fb == NULL || gp2x_fb + 320*240 * bpp / 8 > gp2x_fb_end) {
      if (!invalid_fb_addr) {
        err("fb_thread: %08x is out of range\n", mmsp2.mlc_stl_adr);
        invalid_fb_addr = 1;
      }
      continue;
    }

    mlc_flip(gp2x_fb, bpp);
  }
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
    case 0x1836: // reserved
      d = 0x2330;
      break;
    case 0x2816: // DPC_X_MAX
      d = 319;
      break;
    case 0x2818: // DPC_Y_MAX
      d = 239;
      break;
    case 0x28da:
      d = mmsp2.mlc_stl_cntl;
      break;
    case 0x290e:
    case 0x2912:
      d = mmsp2.mlc_stl_adrl;
      break;
    case 0x2910:
    case 0x2914:
      d = mmsp2.mlc_stl_adrh;
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
  if ((a & 0xfff00000) == 0x7f000000) {
    u32 a_ = a & 0xffff;
    struct timespec ts;
    u64 t64;

    switch (a_) {
    case 0x0a00: // TCOUNT, 1/7372800s
      clock_gettime(CLOCK_REALTIME, &ts);
      t64 = (u64)ts.tv_sec * 1000000000 + ts.tv_nsec;
      // t * 7372800.0 / 1000000000 * 0x100000000 ~= t * 31665935
      t64 *= 31665935;
      d = t64 >> 32;
      break;
    }
  }
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
        // ask for refresh
        pthread_cond_signal(&fb_cond);
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

struct op_linkpage {
  void (*handler)(struct op_context *op_ctx);
  u32 code[0];
};

struct op_stackframe {
  u32 saved_regs[15];
  u32 cpsr;
};

static struct op_linkpage *g_linkpage;
static u32 *g_code_ptr;
static int g_linkpage_count;

#define BIT_SET(v, b) (v & (1 << (b)))

void emu_handle_op(struct op_context *op_ctx, struct op_stackframe *sframe)
{
  u32 *regs = sframe->saved_regs;
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
  g_linkpage->handler = emu_call_handle_op;
  g_code_ptr = g_linkpage->code;
}

static void segv_sigaction(int num, siginfo_t *info, void *ctx)
{
  struct ucontext *context = ctx;
  u32 *regs = (u32 *)&context->uc_mcontext.arm_r0;
  u32 *pc = (u32 *)regs[15];
  struct op_context *op_ctx;
  int i, lp_size;

  if (((regs[15] ^ (u32)&segv_sigaction) & 0xff000000) == 0 ||         // PC is in our segment or
      (((regs[15] ^ (u32)g_linkpage) & ~(LINKPAGE_ALLOC - 1)) == 0) || // .. in linkpage
      ((long)info->si_addr & 0xffe00000) != 0x7f000000)                // faulting not where expected
  {
    // real crash - time to die
    err("segv %d %p @ %08x\n", info->si_code, info->si_addr, regs[15]);
    for (i = 0; i < 8; i++)
      dbg(" r%d=%08x r%2d=%08x\n", i, regs[i], i+8, regs[i+8]);
    signal(num, SIG_DFL);
    raise(num);
    return;
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
  emit_op   (0xe50d0000 + 0xf00 - 4 * 0);                        // str r0, [sp, #(-0xf00 + r0_offs)]
  emit_op   (0xe50de000 + 0xf00 - 4 * 14);                       // str lr, [sp, #(-0xf00 + lr_offs)]
  emit_op   (0xe24f0000 + (g_code_ptr - (u32 *)op_ctx + 2) * 4); // sub r0, pc, #op_ctx
  emit_op   (0xe1a0e00f);                                        // mov lr, pc
  emit_op_io(0xe51ff000, (u32 *)&g_linkpage->handler);           // ldr pc, =handle_op
  emit_op   (0xe51de000 + 0xf00 - 4 * 14);                       // ldr lr, [sp, #(-0xf00 + lr_offs)]
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
  sigaction_t segv_action = {
    .sa_sigaction = segv_sigaction,
    .sa_flags = SA_SIGINFO,
  };
  pthread_t tid;
  void *pret;
  int ret;

  g_linkpage = (void *)(((u32)map_bottom - LINKPAGE_ALLOC) & ~0xfff);
  pret = mmap(g_linkpage, LINKPAGE_ALLOC, PROT_READ|PROT_WRITE,
              MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
  if (pret != g_linkpage) {
    perror(PFX "mmap linkpage");
    exit(1);
  }
  log("linkpages @ %p\n", g_linkpage);
  init_linkpage();

  // host stuff
  ret = host_init();
  if (ret != 0) {
    err("can't init host\n");
    exit(1);
  }

  ret = host_video_init(NULL, 0);
  if (ret != 0) {
    err("can't init host video\n");
    exit(1);
  }

#ifdef WIZ
  // we are short on memmory on Wiz, need special handling
  extern void *host_mmap_upper(void);
  mmsp2.umem = host_mmap_upper();
#else
  mmsp2.umem = mmap(NULL, 0x2000000, PROT_READ|PROT_WRITE|PROT_EXEC,
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif
  if (mmsp2.umem == MAP_FAILED) {
    perror(PFX "mmap upper mem");
    exit(1);
  }

  ret = pthread_create(&tid, NULL, fb_sync_thread, NULL);
  if (ret != 0) {
    err("failed to create fb_sync_thread: %d\n", ret);
    exit(1);
  }
  pthread_detach(tid);

  // mmsp2 defaults
  mmsp2.mlc_stl_adr = 0x03101000; // fb2 is at 0x03381000
  mmsp2.mlc_stl_cntl = 0x4ab; // 16bpp, region 1 active

  sigemptyset(&segv_action.sa_mask);
  sigaction(SIGSEGV, &segv_action, NULL);
}

int emu_read_gpiodev(void *buf, int count)
{
  if (count <= 0) {
    err("gpiodev read %d?\n", count);
    return -1;
  }
  if (count > 4)
    count = 4;

  mmsp2.btn_state = host_read_btns();
  memcpy(buf, &mmsp2.btn_state, count);
  return count;
}

struct dev_fd_t emu_interesting_fds[] = {
  [IFD_SOUND] = { "/dev/dsp", -1 },
  { NULL, 0 },
};

static void *emu_mmap_dev(unsigned int length, int prot, int flags, unsigned int offset)
{
  u8 *umem, *umem_end;

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
  if ((offset & 0xfe000000) != 0x02000000) {
    err("unexpected devmem mmap @ %08x\n", offset);
    errno = EINVAL;
    return MAP_FAILED;
  }

  umem = uppermem_lookup(offset, &umem_end);
  if (umem + length > umem_end)
    err("warning: uppermem @ %08x overflows by %d bytes\n",
        offset, umem + length - umem_end);

  dbg("upper mem @ %08x %d\n", offset, length);
  return umem;
}

void *emu_do_mmap(unsigned int length, int prot, int flags, int fd, unsigned int offset)
{
  if (fd == FAKEDEV_MEM)
    return emu_mmap_dev(length, prot, flags, offset);

  if (fd == FAKEDEV_FB0)
    return emu_mmap_dev(length, prot, flags, offset + 0x03101000);

  if (fd == FAKEDEV_FB1)
    return emu_mmap_dev(length, prot, flags, offset + 0x03381000);

  err("bad/ni mmap(?, %d, %x, %x, %d, %08x)\n", length, prot, flags, fd, offset);
  errno = EINVAL;
  return MAP_FAILED;
}

static int emu_sound_ioctl(int fd, int request, void *argp)
{
  int *arg = argp;

#if 0
  dbg("snd ioctl(%d, %08x, %p)", fd, request, argp);
  if (arg != NULL)
    dbg_c(" [%d]", *arg);
  dbg_c("\n");
#endif

  /* People set strange frag settings on GP2X, which even manage
   * to break audio on pandora (causes writes to fail).
   * Catch this and set to something that works. */
  if (request == SNDCTL_DSP_SPEED) {
    int ret, bsize, frag;

    // ~4ms. gpSP wants small buffers or else it stutters
    // because of it's audio thread sync stuff
    bsize = *arg / 250 * 4;
    for (frag = 0; bsize; bsize >>= 1, frag++)
      ;

    frag |= 16 << 16;       // fragment count
    ret = ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);
    if (ret != 0) {
      err("snd ioctl SETFRAGMENT %08x: ", frag);
      perror(NULL);
    }
  }
  else if (request == SNDCTL_DSP_SETFRAGMENT)
    return 0;

  return ioctl(fd, request, argp);
}

int emu_do_ioctl(int fd, int request, void *argp)
{
  if (fd == emu_interesting_fds[IFD_SOUND].fd)
    return emu_sound_ioctl(fd, request, argp);

  switch (fd) {
  /* *********************** */
  case FAKEDEV_FB0:
  case FAKEDEV_FB1:
    if (argp == NULL)
      goto fail;

    switch (request) {
      case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo *fix = argp;

        memset(fix, 0, sizeof(*fix));
        strcpy(fix->id, "mmsp2_RGB0");
        fix->type         = FB_TYPE_PACKED_PIXELS;
        fix->accel        = FB_ACCEL_NONE;
        fix->visual       = FB_VISUAL_TRUECOLOR;
        fix->line_length  = 320*2;
        fix->smem_start   = (fd == FAKEDEV_FB0) ? 0x03101000 : 0x03381000;
        fix->smem_len     = 320*240*2;
        return 0;
      }
      case FBIOGET_VSCREENINFO: {
        struct fb_var_screeninfo *var = argp;
        static const struct fb_bitfield fbb_red   = { offset: 11, length: 5, };
        static const struct fb_bitfield fbb_green = { offset:  5, length: 6, };
        static const struct fb_bitfield fbb_blue  = { offset:  0, length: 5, };

        memset(var, 0, sizeof(*var));
        var->activate     = FB_ACTIVATE_NOW;
        var->xres         =
        var->xres_virtual = 320;
        var->yres         =
        var->yres_virtual = 240;
        var->width        =
        var->height       = -1;
        var->vmode        = FB_VMODE_NONINTERLACED;
        var->bits_per_pixel = 16;
        var->red          = fbb_red;
        var->green        = fbb_green;
        var->blue         = fbb_blue;
        return 0;
      }
      case FBIOPUT_VSCREENINFO: {
        struct fb_var_screeninfo *var = argp;
        dbg(" put vscreen: %dx%d@%d\n", var->xres, var->yres, var->bits_per_pixel);
        if (var->xres != 320 || var->yres != 240 || var->bits_per_pixel != 16)
          return -1;
        return 0;
      }
    }

  /* *********************** */
  case FAKEDEV_TTY0:
    // fake tty0 to make GPH SDL happy
    if (request == 0x4b46) // KDGKBENT
      return -1;
    return 0;
  }

fail:
  err("bad/ni ioctl(%d, %08x, %p)\n", fd, request, argp);
  errno = EINVAL;
  return -1;
}

static const struct {
  const char *from;
  const char *to;
} path_map[] = {
  { "/mnt/tmp/", "/tmp/" },
};

static const char *wrap_path(const char *path)
{
  char *buff;
  size_t size;
  int i, len;

  // do only path mapping for now
  for (i = 0; i < ARRAY_SIZE(path_map); i++) {
    len = strlen(path_map[i].from);
    if (strncmp(path, path_map[i].from, len) == 0) {
      size = strlen(path) + strlen(path_map[i].to) + 1;
      buff = malloc(size);
      if (buff == NULL)
        break;
      snprintf(buff, size, "%s%s", path_map[i].to, path + len);
      dbg("mapped path \"%s\" -> \"%s\"\n", path, buff);
      return buff;
    }
  }

  return path;
}

void *emu_do_fopen(const char *path, const char *mode)
{
  const char *w_path = wrap_path(path);
  FILE *ret;
  ret = fopen(w_path, mode);
  if (w_path != path)
    free((void *)w_path);
  return ret;
}

// FIXME: threads..
int emu_do_system(const char *command)
{
  static char tmp_path[512];
  const char *p2;
  char *p;

  if (command == NULL)
    return -1;

  // pass through stuff in PATH
  p = strchr(command, ' ');
  p2 = strchr(command, '/');
  if (p2 == NULL || (p != NULL && p2 > p))
    return system(command);

  make_local_path(tmp_path, sizeof(tmp_path), "ginge_prep");
  p = tmp_path + strlen(tmp_path);

  p2 = wrap_path(command);
  snprintf(p, sizeof(tmp_path) - (p - tmp_path), " %s", p2);
  if (p2 != command)
    free((void *)p2);

  dbg("system: \"%s\"\n", tmp_path);
  return system(tmp_path);
}

