/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011,2015
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */

static struct in_default_bind in_evdev_defbinds[] = {
  { KEY_UP,         IN_BINDTYPE_PLAYER12, GP2X_UP },
  { KEY_LEFT,       IN_BINDTYPE_PLAYER12, GP2X_LEFT },
  { KEY_DOWN,       IN_BINDTYPE_PLAYER12, GP2X_DOWN },
  { KEY_RIGHT,      IN_BINDTYPE_PLAYER12, GP2X_RIGHT },
  { KEY_PAGEUP,     IN_BINDTYPE_PLAYER12, GP2X_Y },
  { KEY_END,        IN_BINDTYPE_PLAYER12, GP2X_B },
  { KEY_PAGEDOWN,   IN_BINDTYPE_PLAYER12, GP2X_X },
  { KEY_HOME,       IN_BINDTYPE_PLAYER12, GP2X_A },
  { KEY_RIGHTSHIFT, IN_BINDTYPE_PLAYER12, GP2X_L },
  { KEY_RIGHTCTRL,  IN_BINDTYPE_PLAYER12, GP2X_R },
  { KEY_LEFTALT,    IN_BINDTYPE_PLAYER12, GP2X_START },
  { KEY_LEFTCTRL,   IN_BINDTYPE_PLAYER12, GP2X_SELECT },
  { KEY_COMMA,      IN_BINDTYPE_PLAYER12, GP2X_VOL_DOWN },
  { KEY_DOT,        IN_BINDTYPE_PLAYER12, GP2X_VOL_UP },
  { KEY_1,          IN_BINDTYPE_PLAYER12, GP2X_PUSH },
  { KEY_Q,          IN_BINDTYPE_EMU, 0 },
  { 0, 0, 0 },
};

static const struct in_pdata pandora_evdev_pdata = {
  .defbinds = in_evdev_defbinds,
};

static void host_actions(int actions[IN_BINDTYPE_COUNT])
{
  if (actions[IN_BINDTYPE_EMU] & 1)
    host_forced_exit();
}

static void host_init_input(void)
{
  in_evdev_init(&pandora_evdev_pdata);
}

// vim:shiftwidth=2:expandtab
