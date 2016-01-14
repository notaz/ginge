/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2010-2011
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include "header.h"

#define DL
#include "override.c"

static void next_line(FILE *f)
{
  int c;
  do {
    c = fgetc(f);
  } while (c != EOF && c != '\n');
}

__attribute__((constructor))
static void ginge_init(void)
{
  void *lowest_segments[2] = { NULL, NULL };
  unsigned int start, end;
  int i, ret;
  FILE *f;

  for (i = 0; i < ARRAY_SIZE(real_funcs_np); i++) {
    *real_funcs_np[i].func_ptr = dlsym(RTLD_NEXT, real_funcs_np[i].name);
    if (*real_funcs_np[i].func_ptr == NULL) {
      fprintf(stderr, "dlsym %s: %s\n", real_funcs_np[i].name, dlerror());
      exit(1);
    }
    // dbg("%p %s\n", *real_funcs_np[i].func_ptr, real_funcs_np[i].name);
  }

  f = fopen("/proc/self/maps", "r");
  if (f == NULL) {
    perror("open /proc/self/maps");
    exit(1);
  }

  ret = fscanf(f, "%x-%x ", &start, &end);
  if (ret != 2) {
    perror("parse maps");
    exit(1);
  }
  lowest_segments[0] = (void *)start;

  while (1) {
    next_line(f);

    ret = fscanf(f, "%x-%*s %*s %*s %*s %*s %*s\n", &start);
    if (ret <= 0)
      break;

    if (lowest_segments[0] == NULL || (void *)start < lowest_segments[0])
      lowest_segments[0] = (void *)start;
    else if (lowest_segments[1] == NULL
             && (char *)start - (char *)lowest_segments[0] > 0x800000)
    {
      // an offset is needed because ld-linux also
      // tends to put stuff here
      lowest_segments[1] = (void *)(start - 0x20000);
    }
  }

#if 0
  rewind(f);
  char buff[256];
  while (fgets(buff, sizeof(buff), f))
    fputs(buff, stdout);
#endif
  fclose(f);

  // remove self from preload, further commands (from system() and such)
  // will be handled by ginge_prep.
  unsetenv("LD_PRELOAD");
  unsetenv("LD_LIBRARY_PATH");

  emu_init(lowest_segments, 1);
}

// vim:shiftwidth=2:expandtab
