// vim:shiftwidth=2:expandtab
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/input.h>

#include "header.h"

static int ifd = -1;
static int keystate;

static void init(void)
{
  char buff[64];
  int i;

  for (ifd = -1, i = 0; ; i++) {
    snprintf(buff, sizeof(buff), "/dev/input/event%i", i);
    ifd = open(buff, O_RDONLY | O_NONBLOCK);
    if (ifd == -1)
      break;

    ioctl(ifd, EVIOCGNAME(sizeof(buff)), buff);

    if (strcasestr(buff, "gpio") != NULL)
      break;
    close(ifd);
  }

  if (ifd < 0)
    printf("no input device\n");
}

static const struct {
  unsigned short key;
  unsigned short btn;
} key_map[] = {
  { KEY_LEFT,       GP2X_LEFT },
  { KEY_RIGHT,      GP2X_RIGHT },
  { KEY_UP,         GP2X_UP },
  { KEY_DOWN,       GP2X_DOWN },
  { KEY_PAGEUP,     GP2X_Y },
  { BTN_BASE,       GP2X_Y },
  { KEY_END,        GP2X_B },
  { BTN_BASE2,      GP2X_B },
  { KEY_PAGEDOWN,   GP2X_X },
  { BTN_BASE3,      GP2X_X },
  { KEY_HOME,       GP2X_A },
  { BTN_BASE4,      GP2X_A },
  { KEY_RIGHTSHIFT, GP2X_L },
  { BTN_TL,         GP2X_L },
  { KEY_RIGHTCTRL,  GP2X_R },
  { BTN_TR,         GP2X_R },
  { KEY_LEFTALT,    GP2X_START },
  { BTN_START,      GP2X_START },
  { KEY_LEFTCTRL,   GP2X_SELECT },
  { BTN_SELECT,     GP2X_SELECT },
};

int host_read_btns(void)
{
  struct input_event ev;
  int i, ret;

  if (ifd < 0)
    init();
  if (ifd < 0)
    return keystate;

  while (1)
  {
    ret = read(ifd, &ev, sizeof(ev));
    if (ret < (int) sizeof(ev)) {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        perror("evtest: read error");

      return keystate;
    }

    if (ev.type != EV_KEY)
      continue;

    for (i = 0; i < sizeof(key_map) / sizeof(key_map[0]); i++) {
      if (key_map[i].key != ev.code)
        continue;
      if (ev.value)
        keystate |=  (1 << key_map[i].btn);
      else
        keystate &= ~(1 << key_map[i].btn);
      break;
    }
  }

  return keystate;
}

