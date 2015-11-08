/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/input.h>

#include "../common/warm/warm.h"

extern int memdev, probably_caanoo; // leasing from wiz_video

#define BTN_JOY BTN_JOYSTICK

static struct in_default_bind wiz_evdev_defbinds[] = {
  { KEY_UP,       IN_BINDTYPE_PLAYER12, GP2X_UP },
  { KEY_DOWN,     IN_BINDTYPE_PLAYER12, GP2X_DOWN },
  { KEY_LEFT,     IN_BINDTYPE_PLAYER12, GP2X_LEFT },
  { KEY_RIGHT,    IN_BINDTYPE_PLAYER12, GP2X_RIGHT },
  { BTN_JOY + 0,  IN_BINDTYPE_PLAYER12, GP2X_A },
  { BTN_JOY + 1,  IN_BINDTYPE_PLAYER12, GP2X_X },
  { BTN_JOY + 2,  IN_BINDTYPE_PLAYER12, GP2X_B },
  { BTN_JOY + 3,  IN_BINDTYPE_PLAYER12, GP2X_Y },
  { BTN_JOY + 4,  IN_BINDTYPE_PLAYER12, GP2X_L },
  { BTN_JOY + 5,  IN_BINDTYPE_PLAYER12, GP2X_R },
  { BTN_JOY + 8,  IN_BINDTYPE_PLAYER12, GP2X_START },
  { BTN_JOY + 9,  IN_BINDTYPE_PLAYER12, GP2X_SELECT },
  { BTN_JOY + 10, IN_BINDTYPE_PLAYER12, GP2X_PUSH },
  { BTN_JOY + 6,  IN_BINDTYPE_EMU, 0 },
  { 0, 0, 0 }
};

static const struct in_pdata wiz_evdev_pdata = {
  .defbinds = wiz_evdev_defbinds,
};

// todo: rm when generic code works on Wiz
#if 0
static int gpiodev = -1;

int host_init(void)
{
  gpiodev = open("/dev/GPIO", O_RDONLY);
  if (gpiodev < 0)
    perror(PFX "couldn't open /dev/GPIO");

  return 0;
}

int host_read_btns(void)
{
  int r, value = 0;

  r = read(gpiodev, &value, 4);
  if (value & 0x02)
    value |= 0x05;
  if (value & 0x08)
    value |= 0x14;
  if (value & 0x20)
    value |= 0x50;
  if (value & 0x80)
    value |= 0x41;

  return value;
}
#endif

void *host_mmap_upper(void)
{
  void *ret;
  int r;

  // make sure this never happens on Caanoo
  if (probably_caanoo) {
    err("Wiz mmap code called on Caanoo?");
    return MAP_FAILED;
  }

  // Wiz                GP2X
  // <linux mem>        03460000-03ffffff  00ba0000
  // 02aa0000-02dfffff  03100000-0345ffff  00360000
  // <linux mem>        03000000-030fffff  00100000
  // 03000000-03ffffff  02000000-02ffffff  01000000
  ret = mmap((void *)0x82000000, 0x1000000, PROT_READ|PROT_WRITE|PROT_EXEC,
             MAP_SHARED|MAP_FIXED, memdev, 0x3000000);
  if (ret != (void *)0x82000000)
    goto fail;

  ret = mmap((void *)0x83000000, 0x100000, PROT_READ|PROT_WRITE|PROT_EXEC,
             MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
  if (ret != (void *)0x83000000)
    goto fail;

  ret = mmap((void *)0x83100000, 0x360000, PROT_READ|PROT_WRITE|PROT_EXEC,
             MAP_SHARED|MAP_FIXED, memdev, 0x2aa0000);
  if (ret != (void *)0x83100000)
    goto fail;

  ret = mmap((void *)0x83460000, 0xba0000, PROT_READ|PROT_WRITE|PROT_EXEC,
             MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
  if (ret != (void *)0x83460000)
    goto fail;

  r  = warm_change_cb_range(WCB_B_BIT|WCB_C_BIT, 1, (void *)0x82000000, 0x1000000);
  r |= warm_change_cb_range(WCB_B_BIT|WCB_C_BIT, 1, (void *)0x83100000, 0x360000);
  if (r != 0)
    err("could not make upper mem cacheable.\n");

  return (void *)0x82000000;

fail:
  err("mmap %p: ", ret);
  perror(NULL);
  exit(1);
}

static void host_actions(int actions[IN_BINDTYPE_COUNT])
{
  if (probably_caanoo && (actions[IN_BINDTYPE_EMU] & 1)) {
    // 'home key as Fn' handling
    int act = actions[IN_BINDTYPE_PLAYER12];
    if (act & (1 << GP2X_START)) {
      act &= ~(1 << GP2X_START);
      act |=   1 << GP2X_VOL_UP;
    }
    if (act & (1 << GP2X_SELECT)) {
      act &= ~(1 << GP2X_SELECT);
      act |=   1 << GP2X_VOL_DOWN;
    }
    if (act & (1 << GP2X_Y))
      host_forced_exit();
    actions[IN_BINDTYPE_PLAYER12] = act;
  }
}

static void host_init_input(void)
{
  in_evdev_init(&wiz_evdev_pdata);
}

// vim:shiftwidth=2:expandtab
