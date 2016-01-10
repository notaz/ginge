/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011,2015
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#define _GNU_SOURCE 1 // for plat.c
#include <stdio.h>
#include <stdarg.h>
#include <linux/input.h>

#include "../common/libpicofe/input.h"
#include "../common/libpicofe/linux/in_evdev.h"

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

int host_init(void)
{
  in_init();
  host_init_input();
  in_probe();

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

  snprintf(cmd, sizeof(cmd), "killall %s", g_argv[0]);
  system(cmd);
  usleep(300000);
  snprintf(cmd, sizeof(cmd), "killall -9 %s", g_argv[0]);
  system(cmd);
  exit(status);
}

// vim:shiftwidth=2:expandtab
