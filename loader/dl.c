// vim:shiftwidth=2:expandtab
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include "header.h"

#define DL
#include "override.c"

__attribute__((constructor))
static void ginge_init(void)
{
  unsigned int lowest_segment = (unsigned int)-1;
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

  ret = fscanf(f, "%x-%x %*s %*s %*s %*s %*s\n", &start, &end);
  if (ret != 2) {
    perror("parse maps");
    exit(1);
  }
  lowest_segment = start;

  // assume first entry lists program's text section.
  // unprotect it in case we need some patching.
  ret = mprotect((void *)start, end - start, PROT_READ|PROT_WRITE|PROT_EXEC);
  if (ret != 0)
    perror("warning: mprotect");

  while (1) {
    ret = fscanf(f, "%x-%*s %*s %*s %*s %*s %*s\n", &start);
    if (ret <= 0)
      break;

    if (start < lowest_segment)
      lowest_segment = start;
  }

#if 0
  rewind(f);
  char buff[256];
  while (fgets(buff, sizeof(buff), f))
    fputs(buff, stdout);
#endif
  fclose(f);

  emu_init((void *)lowest_segment);
}

