/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011,2015
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#define _GNU_SOURCE 1 // for plat.c
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#include "../common/libpicofe/input.h"
#include "../common/libpicofe/linux/in_evdev.h"
#include "../common/host_fb.h"

#include "header.h"
#include "realfuncs.h"

// must be affected by realfuncs.h
#include "../common/libpicofe/input.c"
#include "../common/libpicofe/linux/plat.c"
#include "../common/libpicofe/linux/in_evdev.c"

#ifdef PND
#include "host_pnd.c"
#elif defined(WIZ)
#include "host_wiz.c"
#endif

char **g_argv;
static int ts_fd = -1;

/* touscreen. Could just use tsblib, but it's LGPL... */
static int tsc[7];

static int host_ts_init(void)
{
  static const char name_def[] = "/dev/input/touchscreen0";
  const char *name;
  FILE *f;
  int ret;

  f = fopen("/etc/pointercal", "r");
  if (f == NULL) {
    perror("fopen pointercal");
    return -1;
  }
  ret = fscanf(f, "%d %d %d %d %d %d %d", &tsc[0], &tsc[1],
               &tsc[2], &tsc[3], &tsc[4], &tsc[5], &tsc[6]);
  fclose(f);
  if (ret != 7) {
    fprintf(stderr, "could not parse pointercal\n");
    return -1;
  }

  name = getenv("TSLIB_TSDEVICE");
  if (name == NULL)
    name = name_def;
  ts_fd = open(name, O_RDONLY | O_NONBLOCK);
  if (ts_fd == -1) {
    fprintf(stderr, "open %s", name);
    perror("");
    return -1;
  }
  return 0;
}

// returns ranges 0-1023
int host_read_ts(int *pressure, int *x1024, int *y1024)
{
  static int raw_x, raw_y, raw_p;
  struct input_event ev;
  ssize_t ret;

  if (ts_fd == -1)
    return -1;

  for (;;) {
    errno = 0;
    ret = read(ts_fd, &ev, sizeof(ev));
    if (ret != sizeof(ev)) {
      if (errno == EAGAIN)
        break;
      perror("ts read");
      return -1;
    }
    if (ev.type == EV_ABS) {
      switch (ev.code) {
      case ABS_X: raw_x = ev.value; break;
      case ABS_Y: raw_y = ev.value; break;
      case ABS_PRESSURE: raw_p = ev.value; break;
      }
    }
  }

  *pressure = raw_p;
  *x1024 = (tsc[0] * raw_x + tsc[1] * raw_y + tsc[2]) / tsc[6];
  *y1024 = (tsc[3] * raw_x + tsc[4] * raw_y + tsc[5]) / tsc[6];

  host_video_normalize_ts(x1024, y1024);

  return 0;
}

int host_init(void)
{
  in_init();
  host_init_input();
  in_probe();
  host_ts_init();

  return 0;
}

int host_read_btns(void)
{
  int actions[IN_BINDTYPE_COUNT] = { 0, };

  in_update(actions);
  host_actions(actions);

  return actions[IN_BINDTYPE_PLAYER12];
}

void host_forced_exit(int status)
{
  // exit() might not be enough because loader and app data is out of sync,
  // and other threads (which are really processes on this old glibc used)
  // might not exit properly.
  char cmd[64];

  printf("forced exit...\n");

  if (g_argv != NULL) {
    unsetenv("LD_PRELOAD");
    unsetenv("LD_LIBRARY_PATH");

    snprintf(cmd, sizeof(cmd), "killall %s", g_argv[0]);
    system(cmd);
    usleep(300000);
    snprintf(cmd, sizeof(cmd), "killall -9 %s", g_argv[0]);
    system(cmd);
  }
  exit(status);
}

// vim:shiftwidth=2:expandtab
