/*
 * GINGE - GINGE Is Not Gp2x Emulator
 * (C) notaz, 2016
 *
 * This work is licensed under the MAME license, see COPYING file for details.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "syscalls.h"
#include "llibc.h"

// lame, broken and slow, but enough for ginge's needs
static void format_number(char **dst_, int dst_len, unsigned int n,
  char fmt, int justify, int zeropad)
{
  char buf[32], *p = buf, *dst;
  int printing = 0;
  unsigned int div;
  unsigned int t;
  unsigned int w;
  int spaces;
  int neg = 0;
  int left;

  w = justify < 0 ? -justify : justify;
  if (w >= 32)
    w = 31;

  switch (fmt) {
  case 'i':
  case 'd':
    if ((signed int)n < 0) {
      n = -n;
      neg = 1;
    }
  case 'u':
    div = 1000000000;
    left = 10;
    while (w > left) {
      *p++ = ' ';
      w--;
      continue;
    }
    while (left > 0) {
      t = n / div;
      n -= t * div;
      div /= 10;
      if (t || left == 1) {
        if (neg && t && !printing) {
          *p++ = '-';
          if (w > 0) w--;
        }
        printing = 1;
      }
      if (printing)
        *p++ = t + '0';
      else if (w >= left) {
        *p++ = ' ';
        w--;
      }
      left--;
    }
    break;

  case 'p':
    w = 8;
    zeropad = 1;
  case 'x':
    left = 8;
    while (w > left) {
      *p++ = zeropad ? '0' : ' ';
      w--;
      continue;
    }
    while (left > 0) {
      t = n >> (left * 4 - 4);
      t &= 0x0f;
      if (t || left == 1)
        printing = 1;
      if (printing)
        *p++ = t < 10 ? t + '0' : t + 'a' - 10;
      else if (w >= left) {
        *p++ = zeropad ? '0' : ' ';
        w--;
      }
      left--;
    }
    break;

  default:
    memcpy(buf, "<FMTODO>", 9);
    break;
  }
  *p = 0;

  spaces = 0;
  p = buf;
  if (justify < 0) {
    while (*p == ' ') {
      spaces++;
      p++;
    }
  }

  dst = *dst_;
  while (*p != 0 && dst_len > 1) {
    *dst++ = *p++;
    dst_len--;
  }
  while (spaces > 0 && dst_len > 1) {
    *dst++ = ' ';
    spaces--;
    dst_len--;
  }
  *dst = 0;
  *dst_ = dst;
}

int parse_dec(const char **p_)
{
  const char *p = *p_;
  int neg = 0;
  int r = 0;

  if (*p == '-') {
    neg = 1;
    p++;
  }

  while ('0' <= *p && *p <= '9') {
    r = r * 10 + *p - '0';
    p++;
  }

  *p_ = p;
  return neg ? -r : r;
}

void g_fprintf(int fd, const char *fmt, ...)
{
  char buf[256], *d = buf;
  const char *s = fmt;
  int left = sizeof(buf);;
  int justify;
  int zeropad;
  va_list ap;

  va_start(ap, fmt);
  while (*s != 0 && left > 1) {
    if (*s != '%') {
      *d++ = *s++;
      left--;
      continue;
    }
    s++;
    if (*s == 0)
      break;
    if (*s == '%') {
      *d++ = *s++;
      left--;
      continue;
    }

    zeropad = *s == '0';
    justify = parse_dec(&s);
    if (*s == 'l')
      s++; // ignore for now
    if (*s == 's') {
      char *ns = va_arg(ap, char *);
      int len = strlen(ns);
      while (justify > len && left > 1) {
        *d++ = ' ';
        justify--;
        left--;
      }
      if (len > left - 1) {
        memcpy(d, ns, left - 1);
        break;
      }
      memcpy(d, ns, len);
      d += len;
      left -= len;
      while (justify < -len && left > 1) {
        *d++ = ' ';
        justify++;
        left--;
      }
      s++;
      continue;
    }

    format_number(&d, left, va_arg(ap, int), *s++, justify, zeropad);
  }
  *d = 0;
  va_end(ap);

  g_write_raw(fd, buf, d - buf);
}

// vim:shiftwidth=2:expandtab
