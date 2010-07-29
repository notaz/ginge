// vim:shiftwidth=2:expandtab
#ifdef LOADER
#include "../loader/realfuncs.h"
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmn.h"

int make_local_path(char *buf, size_t size, const char *file)
{
  ssize_t ret;
  char *p;

  p = getenv("GINGE_ROOT");
  if (p != NULL) {
    strncpy(buf, p, size);
    buf[size - 1] = 0;
    p = buf + strlen(buf);
  }
  else {
    ret = readlink("/proc/self/exe", buf, size - 1);
    if (ret < 0) {
      perror("readlink");
      goto err;
    }
    buf[ret] = 0;

    p = strrchr(buf, '/');
    if (p == NULL)
      goto err;
    p++;
  }

  snprintf(p, size - (p - buf), "%s", file);

  return 0;

err:
  fprintf(stderr, "ginge: can't determine root, buf: \"%s\"\n", buf);
  return -1;
}

