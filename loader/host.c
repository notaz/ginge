// vim:shiftwidth=2:expandtab

#define _GNU_SOURCE // for plat.c
#include <stdio.h>
#include <stdarg.h>

#include "header.h"
#include "realfuncs.h"

#define IN_EVDEV
#include "../common/common/input.c"
#include "../common/linux/plat.c"
#include "../common/linux/in_evdev.c"

#ifdef PND
#include "host_pnd.c"
#elif defined(WIZ)
#include "host_wiz.c"
#endif

// for plat.c
char **g_argv;

int host_init(void)
{
  in_init();
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

void host_forced_exit(void)
{
  // exit() might not be enough because loader and app data is out of sync,
  // and other threads (which are really processes on this old glibc used)
  // might not exit properly.
  system("killall ginge_sloader");
  usleep(300000);
  system("killall -9 ginge_sloader");
  exit(1);
}
