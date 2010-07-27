// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>

#include "realfuncs.h"
#include "header.h"

#if (DBG & 1)
#define strace printf
#else
#define strace(...)
#endif

#define UNUSED __attribute__((unused))

static const struct dev_fd_t takeover_devs[] = {
  { "/dev/mem",     FAKEDEV_MEM },
  { "/dev/GPIO",    FAKEDEV_GPIO },
  { "/dev/fb0",     FAKEDEV_FB0 },
  { "/dev/fb/0",    FAKEDEV_FB0 },
  { "/dev/fb1",     FAKEDEV_FB1 },
  { "/dev/fb/1",    FAKEDEV_FB1 },
  { "/dev/mmuhack", FAKEDEV_MMUHACK },
  { "/dev/tty",     -1 }, // XXX hmh..
  { "/dev/tty0",    FAKEDEV_TTY0 },
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
  // printf needs valid stack for pthread/errno
  if (((long)&ret & 0xf0000000) == 0xb0000000)
    strace("mmap(%p, %x, %x, %x, %d, %lx) = %p\n", addr, length, prot, flags, fd, (long)offset, ret);
  return ret;
}
#define w_mmap2 w_mmap

static ssize_t w_read(int fd, void *buf, size_t count)
{
  ssize_t ret;
  if (fd == FAKEDEV_GPIO)
    ret = emu_read_gpiodev(buf, count);
  else
    ret = read(fd, buf, count);

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

/* dynamic only */
static UNUSED int w_tcgetattr(int fd, struct termios *termios_p)
{
  int ret;
  if (fd != FAKEDEV_TTY0)
    ret = tcgetattr(fd, termios_p);
  else
    ret = 0;

  strace("tcgetattr(%d, %p) = %d\n", fd, termios_p, ret);
  return ret;
}

static UNUSED int w_tcsetattr(int fd, int optional_actions,
                       const struct termios *termios_p)
{
  int ret;
  if (fd != FAKEDEV_TTY0)
    ret = tcsetattr(fd, optional_actions, termios_p);
  else
    ret = 0;

  strace("tcsetattr(%d, %x, %p) = %d\n", fd, optional_actions, termios_p, ret);
  return ret;
}

static UNUSED FILE *w_fopen(const char *path, const char *mode)
{
  FILE *ret = emu_do_fopen(path, mode);
  strace("fopen(%s, %s) = %p\n", path, mode, ret);
  return ret;
}

static UNUSED int w_system(const char *command)
{
  int ret = emu_do_system(command);
  strace("system(%s) = %d\n", command, ret);
  return ret;
}

#undef open
#undef fopen
#undef mmap
#undef read
#undef ioctl
#undef sigaction
#undef tcgetattr
#undef tcsetattr
#undef system

#ifdef DL

#define MAKE_WRAP_SYM(sym) \
  /* alias wrap symbols to real names */ \
  typeof(sym) sym __attribute__((alias("w_" #sym))); \
  /* wrapper to real functions, to be set up on load */ \
  static typeof(sym) *p_real_##sym

MAKE_WRAP_SYM(open);
MAKE_WRAP_SYM(fopen);
MAKE_WRAP_SYM(mmap);
MAKE_WRAP_SYM(read);
MAKE_WRAP_SYM(ioctl);
MAKE_WRAP_SYM(sigaction);
MAKE_WRAP_SYM(tcgetattr);
MAKE_WRAP_SYM(tcsetattr);
MAKE_WRAP_SYM(system);
typeof(mmap) mmap2 __attribute__((alias("w_mmap")));

#define REAL_FUNC_NP(name) \
  { #name, (void **)&p_real_##name }

static const struct {
  const char *name;
  void **func_ptr;
} real_funcs_np[] = {
  REAL_FUNC_NP(open),
  REAL_FUNC_NP(fopen),
  REAL_FUNC_NP(mmap),
  REAL_FUNC_NP(read),
  REAL_FUNC_NP(ioctl),
  REAL_FUNC_NP(sigaction),
  REAL_FUNC_NP(tcgetattr),
  REAL_FUNC_NP(tcsetattr),
  REAL_FUNC_NP(system),
};

#define open p_real_open
#define fopen p_real_fopen
#define mmap p_real_mmap
#define read p_real_read
#define ioctl p_real_ioctl
#define sigaction p_real_sigaction
#define tcgetattr p_real_tcgetattr
#define tcsetattr p_real_tcsetattr
#define system p_real_system

#undef MAKE_WRAP_SYM
#undef REAL_FUNC_NP

#endif

// just call real funcs for static, pointer for dynamic
int real_open(const char *pathname, int flags, mode_t mode)
{
  return open(pathname, flags, mode);
}

FILE *real_fopen(const char *path, const char *mode)
{
  return fopen(path, mode);
}

void *real_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  return mmap(addr, length, prot, flags, fd, offset);
}

int real_read(int fd, void *buf, size_t count)
{
  return read(fd, buf, count);
}

int real_ioctl(int d, int request, void *argp)
{
  return ioctl(d, request, argp);
}

int real_sigaction(int signum, const sigaction_t *act, sigaction_t *oldact)
{
  return sigaction(signum, act, oldact);
}

int real_tcgetattr(int fd, struct termios *termios_p)
{
  return tcgetattr(fd, termios_p);
}

int real_tcsetattr(int fd, int optional_actions,
                       const struct termios *termios_p)
{
  return tcsetattr(fd, optional_actions, termios_p);
}

int real_system(const char *command)
{
  return system(command);
}
