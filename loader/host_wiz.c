// vim:shiftwidth=2:expandtab
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "header.h"
#include "../common/warm.h"
#include "realfuncs.h"

static int gpiodev = -1;
extern int memdev; // leasing from wiz_video

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

void *host_mmap_upper(void)
{
  void *ret;
  int r;

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

