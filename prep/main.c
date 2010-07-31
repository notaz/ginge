// vim:shiftwidth=2:expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <elf.h>
#include <sys/stat.h>
#include <ctype.h>

#include "../common/host_fb.h"
#include "../common/cmn.h"

#define PFX "ging_prep: "
#define LOADER_STATIC   "ginge_sloader"
#define LOADER_DYNAMIC  "ginge_dyn.sh"
#define LAUNCHER        "gp2xmenu"

#ifdef PND
#define WRAP_APP        "op_runfbapp "
#else
#define WRAP_APP        ""
#endif

#include "font.c"

static void *fb_mem;
static int fb_stride;
static int fb_x, fb_y;
static int init_done;

static char *sskip(char *p)
{
  while (p && *p && isspace(*p))
    p++;
  return p;
}

static char *cskip(char *p)
{
  while (p && *p && !isspace(*p))
    p++;
  return p;
}

static void fb_text_init(void)
{
  int ret = host_video_init(&fb_stride, 1);
  if (ret == 0)
    fb_mem = host_video_flip();
  fb_x = 4;
  fb_y = 4;
  init_done = 1;
}

static void fb_syms_out(void *fbi, int x, int y, int dotsz, int stride, const char *text, int count)
{
  int v = -1, n = 0, *p;
  int i, l;
  char *fb;

  fb = (char *)fbi + x * dotsz + y * stride;

  for (i = 0; i < count; i++)
  {
    for (l = 0; l < 8; l++)
    {
      #define pix(fdmask,add) \
        p = (fontdata8x8[((text[i])*8)+l] & fdmask) ? &v : &n; \
        memcpy(fb + l*stride + add*dotsz, p, dotsz)
      pix(0x80,  0);
      pix(0x40,  1);
      pix(0x20,  2);
      pix(0x10,  3);
      pix(0x08,  4);
      pix(0x04,  5);
      pix(0x02,  6);
      pix(0x01,  7);
      #undef pix
    }
    fb += dotsz * 8;
  }
}

// FIXME: y overrun
static void fb_text_out(char *text)
{
  int dotsz = 2, w = 320; // hardcoded for now
  char *p, *pe;
  int l;

  if (!init_done)
    fb_text_init();

  if (fb_mem == NULL)
    return;

  p = text;
  while (*p) {
    for (; *p && isspace(*p); p++) {
      if (*p == '\n' || fb_x + dotsz * 8 > w) {
        fb_x = 4;
        fb_y += 8;
      }
      if (*p >= 0x20)
        fb_x += 8;
    }

    pe = cskip(p);
    l = pe - p;
    if (fb_x + 8 * l > w) {
      fb_x = 4;
      fb_y += 8;
    }
    fb_syms_out(fb_mem, fb_x, fb_y, dotsz, fb_stride, p, l);
    fb_x += 8 * l;
    p = pe;
  }
}

static void fbprintf(int is_err, const char *format, ...)
{
  va_list ap;
  char buff[512];

  va_start(ap, format);
  vsnprintf(buff, sizeof(buff), format, ap);
  va_end(ap);
  fputs(buff, is_err ? stderr : stdout);

  fb_text_out(buff);
}

#define msg(fmt, ...) fbprintf(0, fmt, #__VA_ARGS__)
#define err(fmt, ...) fbprintf(1, fmt, #__VA_ARGS__)

static int id_elf(const char *fname)
{
  Elf32_Ehdr hdr;
  Elf32_Phdr *phdr = NULL;
  FILE *fi;
  int i, ret = 0;

  fi = fopen(fname, "rb");
  if (fi == NULL) {
    err("open %s: ", fname);
    perror("");
    return -1;
  }

  if (fread(&hdr, 1, sizeof(hdr), fi) != sizeof(hdr))
    goto out;

  if (memcmp(hdr.e_ident, ELFMAG "\x01\x01", SELFMAG + 2) != 0)
    goto out;

  if (hdr.e_phentsize != sizeof(Elf32_Phdr) || hdr.e_phnum == 0)
    goto out;

  phdr = malloc(hdr.e_phnum * hdr.e_phentsize);
  if (phdr == NULL)
    goto out;

  if (fread(phdr, hdr.e_phentsize, hdr.e_phnum, fi) != hdr.e_phnum)
    goto out;

  ret = 1;

  // do what 'file' does - check for PT_INTERP in program headers
  for (i = 0; i < hdr.e_phnum; i++) {
    if (phdr[i].p_type == PT_INTERP) {
      ret = 2;
      break;
    }
  }

out:
  fclose(fi);
  free(phdr);
  return ret;
}

static void dump_args(FILE *fout, int argc, char * const argv[])
{
  const char *p;
  int i;

  for (i = 0; i < argc; i++) {
    if (i != 0)
      fputc(' ', fout);
    fputc('"', fout);

    for (p = argv[i]; *p != 0; p++) {
      if (*p == '"')
        fputc('\\', fout);
      fputc(*p, fout);
    }

    fputc('"', fout);
  }
}

static char *get_arg(char *d, size_t size, char *p)
{
  char *pe;
  int len;

  p = sskip(p);
  pe = cskip(p);
  len = pe - p;

  if (len > size - 1) {
    err(PFX "get_arg: buff to small: %d/%d\n", len, size);
    len = size - 1;
  }
  strncpy(d, p, len);
  d[len] = 0;

  return sskip(pe);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define CB_ENTRY(x) { x, sizeof(x) - 1 }
const struct {
  const char *name;
  int len;
} conv_blacklist[] = {
  CB_ENTRY("insmod"),
  CB_ENTRY("modprobe"),
  CB_ENTRY("umount"),
  CB_ENTRY("./cpuctrl_tiny"),
};

static int cmd_in_blacklist(char *cmd)
{
  int i;

  cmd = sskip(cmd);
  for (i = 0; i < ARRAY_SIZE(conv_blacklist); i++)
    if (strncmp(cmd, conv_blacklist[i].name, conv_blacklist[i].len) == 0)
      return 1;

  return 0;
}

int main(int argc, char *argv[])
{
  static const char out_script[] = "/tmp/ginge_conv.sh";
  char root_path[512], cwd[512];
  int have_cramfs = 0;
  int rerun_gp2xmenu = 1;
  FILE *fin, *fout;
  int ret;

  if (argc < 2) {
    err("usage: %s <script|program> [args]\n", argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "--cleanup") == 0) {
    // as loader may crash eny time, restore screen for them menu
    host_video_init(NULL, 0);
    host_video_finish();
    return 0;
  }

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    err(PFX "failed to get cwd\n");
    return 1;
  }

  ret = make_local_path(root_path, sizeof(root_path), "");
  if (ret != 0) {
    err(PFX "failed to generate root path\n");
    return 1;
  }

  fout = fopen(out_script, "w");
  if (fout == NULL) {
    perror("can't open output script");
    return 1;
  }

  fprintf(fout, "#!/bin/sh\n");

  ret = id_elf(argv[1]);
  if (ret == 1 || ret == 2) {
    if (cmd_in_blacklist(argv[1])) {
      fprintf(stderr, "blacklisted: %s\n", argv[1]);
      goto no_in_script;
    }
  }

  switch (ret) {
  case 0:
    break;

  case 1:
    fprintf(fout, WRAP_APP "%s%s ", root_path, LOADER_STATIC);
    dump_args(fout, argc - 1, &argv[1]);
    fprintf(fout, "\n");
    goto no_in_script;

  case 2:
    fprintf(fout, WRAP_APP "%s%s \"%s\" ", root_path, LOADER_DYNAMIC, root_path);
    dump_args(fout, argc - 1, &argv[1]);
    fprintf(fout, "\n");
    goto no_in_script;

  default:
    return 1;
  }

  // assume script
  fin = fopen(argv[1], "r");
  if (fin == NULL)
    return 1;

  while (1) {
    char buff[512], fname[512], *p, *p2;

    p = fgets(buff, sizeof(buff), fin);
    if (p == NULL)
      break;
    p = sskip(p);

    if (p[0] == '#' && p[1] == '!')
      continue;

    if (*p == 0) {
      fputs("\n", fout);
      continue;
    }

    // things we are sure we want to pass
    if (*p == '#' || strncmp(p, "export ", 7) == 0)
      goto pass;

    // hmh..
    if (strncmp(p, "exec ", 5) == 0)
      p = sskip(p + 5);

    // blacklist some stuff
    if      (strncmp(p, "/sbin/", 6) == 0)
      p2 = p + 6;
    else if (strncmp(p, "/bin/", 5) == 0)
      p2 = p + 5;
    else
      p2 = p;
    if (strncmp(p2, "mount ", 6) == 0) {
      p2 = sskip(p2 + 6);
      // cramfs stuff?
      if (strstr(p2, "cramfs")) {
        while (*p2 == '-') {
          // skip option
          p2 = sskip(cskip(p2));
          p2 = sskip(cskip(p2));
        }
        if (*p2 == 0) {
          err(PFX "cramfs: missing mount file in \"%s\"?\n", p);
          continue;
        }
        p2 = get_arg(fname, sizeof(fname), p2);
        if (*p2 == 0) {
          err(PFX "cramfs: missing mount point in \"%s\"?\n", p);
          continue;
        }
        get_arg(buff, sizeof(buff), p2);
        fprintf(fout, "if [ `ls %s | wc -l` -eq 0 ]; then\n", buff);
        fprintf(fout, "  rmdir \"%s\"\n", buff); // cramfsck doesn't want it
        fprintf(fout, "  %stools/cramfsck -x \"%s\" \"%s\"\n", root_path, buff, fname);
        fprintf(fout, "fi\n");
        have_cramfs = 1;
      }
      continue;
    }
    if (cmd_in_blacklist(p2))
      continue;

    // cd?
    if (strncmp(p, "cd ", 3) == 0) {
      get_arg(fname, sizeof(fname), p + 3);
      if (strncmp(fname, "/usr/gp2x", 9) == 0)
        continue;
      ret = chdir(fname);
      if (ret != 0) {
        err("%s: ", fname);
        perror("");
      }
    }

    // trying to run something from cwd?
    if ((p[0] == '.' && p[1] == '/') || *p == '/') {
      get_arg(fname, sizeof(fname), p);
      p2 = strrchr(fname, '/');
      if (p2 != NULL && strcmp(p2 + 1, "gp2xmenu") == 0)
        continue;

      ret = id_elf(fname);
      switch (ret) {
      case 1:
        printf(PFX "prefixing as static: %s", p);
        fprintf(fout, WRAP_APP "%s%s ", root_path, LOADER_STATIC);
        break;

      case 2:
        printf(PFX "prefixing as dynamic: %s", p);
        fprintf(fout, WRAP_APP "%s%s \"%s\" ", root_path, LOADER_DYNAMIC, root_path);
        break;

      default:
        break;
      }
    }

pass:
    fputs(p, fout);
  }

  fclose(fin);

no_in_script:
#ifdef WIZ
  fprintf(fout, "sync\n");
  fprintf(fout, "%sginge_prep --cleanup\n", root_path);
#endif
  if (rerun_gp2xmenu) {
    fprintf(fout, "cd %s\n", root_path);
    fprintf(fout, "exec %s%s\n", root_path, LAUNCHER);
  }

  fclose(fout);

  //msg("starting script..\n");
  if (have_cramfs) {
    msg("\nsome files need to be unpacked, this may tike a few minutes.\n");
#ifdef PND
    msg("Please wait at least while SD LED is active.\n");
#endif
  }
  system("echo ---; cat /tmp/ginge_conv.sh; echo ---");
  chmod(out_script, S_IRWXU|S_IRWXG|S_IRWXO);
  chdir(cwd);
  execlp(out_script, out_script, NULL);
  perror("run out_script");

  return 1;
}

