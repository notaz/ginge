/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011,2016
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>

#include "realfuncs.h"
#include "syscalls.h"
#include "llibc.h"
#include "header.h"

#if (DBG & 1)
#define strace g_printf
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
  { "/dev/tty",     FAKEDEV_TTY0 },
  { "/dev/tty0",    FAKEDEV_TTY0 },
  { "/dev/touchscreen/wm97xx", FAKEDEV_WM97XX },
  { "/etc/pointercal",         FAKEDEV_WM97XX_P },
#ifdef PND
  { "/dev/input/event*", -1 }, // hide for now, may cause dupe events
#endif
};

static long w_open_raw(const char *pathname, int flags, mode_t mode)
{
  long ret;
  int i;

  for (i = 0; i < ARRAY_SIZE(takeover_devs); i++) {
    const char *p, *oname;
    int len;

    oname = takeover_devs[i].name;
    p = strchr(oname, '*');
    if (p != NULL)
      len = p - oname;
    else
      len = strlen(oname) + 1;

    if (strncmp(pathname, oname, len) == 0) {
      ret = takeover_devs[i].fd;
      break;
    }
  }

  if (i == ARRAY_SIZE(takeover_devs))
    ret = g_open_raw(pathname, flags, mode);

  if (ret >= 0) {
    for (i = 0; emu_interesting_fds[i].name != NULL; i++) {
      struct dev_fd_t *eifd = &emu_interesting_fds[i];
      if (strcmp(pathname, eifd->name) == 0) {
        eifd->fd = ret;
        if (eifd->open_cb != NULL)
          eifd->open_cb(ret);
        break;
      }
    }
  }

  strace("open(%s) = %ld\n", pathname, ret);
  return ret;
}

static int w_open(const char *pathname, int flags, mode_t mode)
{
  long ret = w_open_raw(pathname, flags, mode);
  return g_syscall_error(ret);
}

static long w_mmap_raw(void *addr, size_t length, int prot, int flags,
  int fd, off_t offset)
{
  long ret;

  if (FAKEDEV_MEM <= fd && fd < FAKEDEV_FD_BOUNDARY)
    ret = emu_do_mmap(length, prot, flags, fd, offset);
  else
    ret = g_mmap2_raw(addr, length, prot, flags, fd, offset >> 12);

  strace("mmap(%p, %x, %x, %x, %d, %lx) = %lx\n",
         addr, length, prot, flags, fd, (long)offset, ret);
  return ret;
}

static long w_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
  long ret = w_mmap_raw(addr, length, prot, flags, fd, offset);
  return g_syscall_error(ret);
}
#define w_mmap2 w_mmap

static long w_munmap_raw(void *addr, size_t length)
{
  long ret;

  ret = emu_do_munmap(addr, length);
  if (ret == -EAGAIN)
    ret = g_munmap_raw(addr, length);

  strace("munmap(%p, %x) = %ld\n", addr, length, ret);
  return ret;
}

static int w_munmap(void *addr, size_t length)
{
  long ret = w_munmap_raw(addr, length);
  return g_syscall_error(ret);
}

long w_read_raw(int fd, void *buf, size_t count)
{
  long ret;

  if (FAKEDEV_MEM <= fd && fd < FAKEDEV_FD_BOUNDARY)
    ret = emu_do_read(fd, buf, count);
  else
    ret = g_read_raw(fd, buf, count);

  //strace("read(%d, %p, %zd) = %ld\n", fd, buf, count, ret);
  return ret;
}

static ssize_t w_read(int fd, void *buf, size_t count)
{
  long ret = w_read_raw(fd, buf, count);
  return g_syscall_error(ret);
}

long w_ioctl_raw(int fd, int request, void *argp)
{
  long ret;

  if ((FAKEDEV_MEM <= fd && fd < FAKEDEV_FD_BOUNDARY) ||
      fd == emu_interesting_fds[IFD_SOUND].fd)
    ret = emu_do_ioctl(fd, request, argp);
  else
    ret = g_ioctl_raw(fd, request, argp);

  strace("ioctl(%d, %08x, %p) = %ld\n", fd, request, argp, ret);
  return ret;
}

static int w_ioctl(int fd, int request, void *argp)
{
  long ret = w_ioctl_raw(fd, request, argp);
  return g_syscall_error(ret);
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

extern char **environ;

static UNUSED int w_execl(const char *path, const char *arg, ...)
{
  // don't allow exec (for now)
  strace("execl(%s, %s, ...) = ?\n", path, arg);
  exit(1);
}

static UNUSED int w_execlp(const char *file, const char *arg, ...)
{
  strace("execlp(%s, %s, ...) = ?\n", file, arg);
  exit(1);
}

static UNUSED int w_execle(const char *path, const char *arg, ...)
{
  strace("execle(%s, %s, ...) = ?\n", path, arg);
  exit(1);
}

static UNUSED int w_execv(const char *path, char *const argv[])
{
  strace("execv(%s, %p) = ?\n", path, argv);
  return emu_do_execve(path, argv, environ);
}

static UNUSED int w_execvp(const char *file, char *const argv[])
{
  strace("execvp(%s, %p) = ?\n", file, argv);
  return emu_do_execve(file, argv, environ);
}

long w_execve_raw(const char *filename, char * const argv[],
                  char * const envp[])
{
  strace("execve(%s, %p, %p) = ?\n", filename, argv, envp);
  return emu_do_execve(filename, argv, envp);
}

static UNUSED int w_execve(const char *filename, char * const argv[],
                           char * const envp[])
{
  long ret = w_execve_raw(filename, argv, envp);
  return g_syscall_error(ret);
}

static int w_chdir(const char *path)
{
  long ret;

  if (path != NULL && strstr(path, "/usr/gp2x") != NULL)
    ret = 0;
  else
    ret = g_chdir_raw(path);

  strace("chdir(%s) = %ld\n", path, ret);
  return g_syscall_error(ret);
}

static ssize_t w_readlink(const char *path, char *buf, size_t bufsiz)
{
  long ret;

#ifndef DL
  if (path != NULL && strncmp(path, "/proc/", 6) == 0
      && strcmp(strrchr(path, '/'), "/exe") == 0)
  {
    ret = snprintf(buf, bufsiz, "%s", bin_path);
    if (ret > bufsiz)
      ret = bufsiz;
  }
  else
#endif
    ret = g_readlink_raw(path, buf, bufsiz);

  strace("readlink(%s, %s, %zd) = %ld\n", path, buf, bufsiz, ret);
  return g_syscall_error(ret);
}

#undef open
#undef fopen
#undef mmap
#undef munmap
#undef read
#undef ioctl
#undef close
#undef sigaction
#undef tcgetattr
#undef tcsetattr
#undef system
#undef execl
#undef execlp
#undef execle
#undef execv
#undef execvp
#undef execve
#undef chdir
#undef readlink

#ifdef DL

#define MAKE_WRAP_SYM_N(sym) \
  /* alias wrap symbols to real names */ \
  typeof(sym) sym __attribute__((alias("w_" #sym)))

#define MAKE_WRAP_SYM(sym) \
  MAKE_WRAP_SYM_N(sym); \
  /* wrapper to real functions, to be set up on load */ \
  static typeof(sym) *p_real_##sym

// note: update symver too
MAKE_WRAP_SYM_N(open);
MAKE_WRAP_SYM(fopen);
MAKE_WRAP_SYM_N(mmap);
MAKE_WRAP_SYM_N(munmap);
MAKE_WRAP_SYM_N(read);
MAKE_WRAP_SYM_N(ioctl);
MAKE_WRAP_SYM(sigaction);
MAKE_WRAP_SYM(tcgetattr);
MAKE_WRAP_SYM(tcsetattr);
MAKE_WRAP_SYM(system);
MAKE_WRAP_SYM_N(execl);
MAKE_WRAP_SYM_N(execlp);
MAKE_WRAP_SYM_N(execle);
MAKE_WRAP_SYM_N(execv);
MAKE_WRAP_SYM_N(execvp);
MAKE_WRAP_SYM_N(execve);
MAKE_WRAP_SYM_N(chdir);
MAKE_WRAP_SYM_N(readlink);
typeof(mmap) mmap2 __attribute__((alias("w_mmap")));

#define REAL_FUNC_NP(name) \
  { #name, (void **)&p_real_##name }

static const struct {
  const char *name;
  void **func_ptr;
} real_funcs_np[] = {
  //REAL_FUNC_NP(open),
  REAL_FUNC_NP(fopen),
  //REAL_FUNC_NP(mmap),
  //REAL_FUNC_NP(munmap),
  //REAL_FUNC_NP(read),
  //REAL_FUNC_NP(ioctl),
  REAL_FUNC_NP(sigaction),
  REAL_FUNC_NP(tcgetattr),
  REAL_FUNC_NP(tcsetattr),
  REAL_FUNC_NP(system),
  // exec* - skipped
  //REAL_FUNC_NP(execve),
  //REAL_FUNC_NP(chdir),
};

//#define open p_real_open
#define fopen p_real_fopen
//#define mmap p_real_mmap
//#define munmap p_real_munmap
//#define read p_real_read
//#define ioctl p_real_ioctl
#define sigaction p_real_sigaction
#define tcgetattr p_real_tcgetattr
#define tcsetattr p_real_tcsetattr
#define system p_real_system
//#define execve p_real_execve
//#define chdir p_real_chdir

#undef MAKE_WRAP_SYM
#undef REAL_FUNC_NP

#endif

// just call real funcs for static, pointer for dynamic
int real_open(const char *pathname, int flags, ...)
{
  va_list ap;
  mode_t mode;
  long ret;

  va_start(ap, flags);
  mode = va_arg(ap, mode_t);
  va_end(ap);
  ret = g_open_raw(pathname, flags, mode);
  return g_syscall_error(ret);
}

FILE *real_fopen(const char *path, const char *mode)
{
  return fopen(path, mode);
}

void *real_mmap(void *addr, size_t length, int prot, int flags,
  int fd, off_t offset)
{
  long ret = g_mmap2_raw(addr, length, prot, flags, fd, offset >> 12);
  return (void *)g_syscall_error(ret);
}

int real_munmap(void *addr, size_t length)
{
  return g_syscall_error(g_munmap_raw(addr, length));
}

int real_read(int fd, void *buf, size_t count)
{
  long ret = g_read_raw(fd, buf, count);
  return g_syscall_error(ret);
}

int real_ioctl(int fd, int request, void *argp)
{
  long ret = g_ioctl_raw(fd, request, argp);
  return g_syscall_error(ret);
}

int real_close(int fd)
{
  long ret = g_close_raw(fd);
  return g_syscall_error(ret);
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

// real_exec* is missing intentionally - we don't need them

int real_execve(const char *filename, char *const argv[],
                  char *const envp[])
{
  long ret = g_execve_raw(filename, argv, envp);
  return g_syscall_error(ret);
}

int real_chdir(const char *path)
{
  long ret = g_chdir_raw(path);
  return g_syscall_error(ret);
}

ssize_t real_readlink(const char *path, char *buf, size_t bufsiz)
{
  long ret = g_readlink_raw(path, buf, bufsiz);
  return g_syscall_error(ret);
}

void real_sleep(unsigned int seconds)
{
  struct timespec ts = { seconds, 0 };
  g_nanosleep_raw(&ts, NULL);
}

void real_usleep(unsigned int usec)
{
  struct timespec ts = { usec / 1000000, usec % 1000000 };
  g_nanosleep_raw(&ts, NULL);
}

void real_exit(int status)
{
  g_exit_group_raw(status);
}

// vim:shiftwidth=2:expandtab
