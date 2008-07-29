/* putter.c */

/*
    Copyright (C) 2008 Micah Cowan

    This file is part of GNU teseq.

    GNU teseq is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    GNU teseq is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "teseq.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "putter.h"

#define DEFAULT_LINE_MAX        78

struct putter
{
  FILE *file;
  size_t nc;
  const char *presep;
  size_t presz;
  const char *postsep;
  size_t postsz;
  size_t linemax;
};

struct putter *
putter_new (FILE * file)
{
  struct putter *p = malloc (sizeof *p);
  p->file = file;
  p->nc = 0;
  p->linemax = DEFAULT_LINE_MAX;
  p->presep = p->postsep = "";
  p->presz = 0;
  p->postsz = 0;
  return p;
}

void
putter_delete (struct putter *p)
{
  free (p);
}

static void
ensure_space (struct putter *p, size_t addition)
{
  if (p->nc + addition > p->linemax || p->nc + p->presz == p->linemax)
    {
      fprintf (p->file, "%s\n%s", p->presep, p->postsep);
      p->nc = p->postsz;
    }
  p->nc += addition;
}

int
putter_start (struct putter *p, const char *s,
              const char *pre, const char *post)
{
  p->presep = pre;
  p->postsep = post;
  p->presz = strlen (pre);
  p->postsz = strlen (post);

  if (p->nc > 0)
    putc ('\n', p->file);
  p->nc = strlen (s);
  return fputs (s, p->file);
}

int
putter_finish (struct putter *p, const char *s)
{
  p->presep = "";
  p->postsep = "";
  p->presz = 0;
  p->postsz = 0;

  if (p->nc == 0)
    return 0;
  if (p->nc + strlen (s) > p->linemax)
    fprintf (p->file, "%s\n", p->presep);
  p->nc = 0;
  return fprintf (p->file, "%s\n", s);
}

int
putter_putc (struct putter *p, unsigned char c)
{
  ensure_space (p, 1);
  return putc (c, p->file);
}

int
putter_puts (struct putter *p, const char *s)
{
  ensure_space (p, strlen (s));
  return fputs (s, p->file);
}

int
putter_printf (struct putter *p, const char *fmt, ...)
{
  int len, ret;
  va_list ap;

  va_start (ap, fmt);
  len = vsnprintf (NULL, 0, fmt, ap);
  va_end (ap);
  ensure_space (p, len);
  va_start (ap, fmt);
  ret = vfprintf (p->file, fmt, ap);
  va_end (ap);
  return ret;
}

/* Combines:
 *      putter_start (p, "", "", "");
 *      putter_printf (p, fmt, ...);
 *      putter_finish (p, ""); */
int
putter_single (struct putter *p, const char *fmt, ...)
{
  va_list ap;
  int ret, e;

  if (p->nc > 0)
    putc ('\n', p->file);
  va_start (ap, fmt);
  ret = vfprintf (p->file, fmt, ap);
  va_end (ap);
  p->presep = "";
  p->postsep = "";
  p->presz = 0;
  p->postsz = 0;
  p->nc = 0;
  e = putc ('\n', p->file);
  if (e == -1)
    return -1;

  return ret + 1;
}
