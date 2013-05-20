/* putter.c */

/*
    Copyright (C) 2008,2013 Micah Cowan

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

#include <errno.h>
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
  struct sgr_def *sgr;
  struct sgr_def *sgr_decor;

  putter_error_handler handler;
  void *handler_arg;
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
  p->handler = NULL;
  p->handler_arg = NULL;
  p->sgr = NULL;
  p->sgr_decor = NULL;
  return p;
}

#define HANDLER_IF(p, cond)                     \
  do                                            \
    {                                           \
      if ((p)->handler && cond)                 \
        (p)->handler (errno, (p)->handler_arg); \
    }                                           \
  while (0)

#define HANDLE_ERROR(p, action, cond)           \
  do                                            \
    {                                           \
      errno = 0;                                \
      action;                                   \
      HANDLER_IF(p, cond);                      \
    }                                           \
  while (0)

#define BRACED(p) \
  ((p)->presep != NULL && (p)->presep[0] != '\0')

static struct sgr_def sgr0 = { "", 0 };

static void
do_color (struct putter *p, struct sgr_def *sgr)
{
  int e;

  if (configuration.color != CFG_COLOR_ALWAYS || sgr == NULL)
    return;

  HANDLE_ERROR
    (
      p,
      e = fprintf (p->file, "\033[%.*sm", sgr->len, sgr->sgr),
      e < 0
    );
}


void
putter_set_handler (struct putter *p, putter_error_handler f, void *arg)
{
  p->handler = f;
  p->handler_arg = arg;
}


void
putter_delete (struct putter *p)
{
  free (p);
}

static void
ensure_space (struct putter *p, size_t addition)
{
  errno = 0;
  if (p->nc + addition > p->linemax || p->nc + p->presz == p->linemax)
    {
      int cs;
      if (BRACED (p))
        {
          do_color (p, &sgr0);
          do_color (p, p->sgr_decor);
          HANDLE_ERROR
            (
              p,
              cs = fprintf (p->file, "%s\n%s", p->presep, p->postsep),
              cs < 0
            );
          if (p->sgr_decor)
            do_color(p, &sgr0);
          do_color(p, p->sgr);
        }
      else 
        {
          do_color (p, &sgr0);
          HANDLE_ERROR
            (
              p,
              cs = fputc ('\n', p->file),
              cs == EOF
            );
          do_color(p, p->sgr);
          HANDLE_ERROR
            (
              p,
              cs = fputs (p->postsep, p->file),
              cs < 0
            );
        }
      p->nc = p->postsz;
    }
  p->nc += addition;
}

void
putter_start (struct putter *p, struct sgr_def *sgr,
              struct sgr_def *sgr_decor, /* Only used for text decorations. */
              const char *s, const char *pre, const char *post)
{
  int e;

  p->presep = pre;
  p->postsep = post;
  p->presz = strlen (pre);
  p->postsz = strlen (post);
  p->sgr = sgr;
  p->sgr_decor = sgr_decor;

  if (p->sgr_decor && p->sgr_decor->len == 0)
    p->sgr_decor = NULL;

  if (p->nc > 0)
    {
      HANDLE_ERROR
        (
          p,
          e = putc ('\n', p->file),
          e == EOF
        );
    }
  p->nc = strlen (s);
  errno = 0;
  if (BRACED (p))
    do_color (p, p->sgr_decor);
  else
    do_color (p, p->sgr);
  HANDLE_ERROR
    (
      p,
      e = fputs (s, p->file),
      e == EOF
    );
  if (BRACED (p))
    {
      if (p->sgr_decor)
        do_color (p, &sgr0);
      do_color (p, p->sgr);
    }
}

void
putter_finish (struct putter *p, const char *s)
{
  int cs;

  if (p->nc == 0)
    return;
  
  p->nc = 0;
  do_color (p, &sgr0);
  if (BRACED (p))
    do_color (p, p->sgr_decor);
  HANDLE_ERROR
    (
      p,
      cs = fprintf (p->file, "%s", s),
      cs < 0
    );
  if (BRACED (p) && p->sgr_decor)
    do_color (p, &sgr0);
  HANDLE_ERROR
    (
      p,
      cs = fputc ('\n', p->file),
      cs < 0
    );
  
  p->presep = "";
  p->postsep = "";
  p->presz = 0;
  p->postsz = 0;
  p->sgr = NULL;
  p->sgr_decor = NULL;
}

void
putter_putc (struct putter *p, unsigned char c)
{
  int e;

  ensure_space (p, 1);
  HANDLE_ERROR
    (
      p,
      e = putc (c, p->file),
      e == EOF
    );
}

void
putter_puts (struct putter *p, const char *s)
{
  int e;
  
  ensure_space (p, strlen (s));
  HANDLE_ERROR
    (
      p,
      e = fputs (s, p->file),
      e == EOF
    );
}

void
putter_printf (struct putter *p, const char *fmt, ...)
{
  int len, ret, serr;
  va_list ap;

  va_start (ap, fmt);
  len = vsnprintf (NULL, 0, fmt, ap);
  va_end (ap);
  ensure_space (p, len);
  va_start (ap, fmt);
  errno = 0;
  ret = vfprintf (p->file, fmt, ap);
  serr = errno;
  va_end (ap);
  errno = serr;
  HANDLER_IF (p, ret < 0);
  return;
}

/* Combines:
 *      putter_start (p, "", "", "");
 *      putter_printf (p, fmt, ...);
 *      putter_finish (p, ""); */
static void
vsingle (struct putter *p, struct sgr_def *sgr,
         const char *pfx, const char *fmt, va_list ap)
{
  int e;

  if (p->nc > 0)
    {
      do_color (p, &sgr0);
      HANDLE_ERROR
        (
          p,
          e = putc ('\n', p->file),
          e == EOF
        );
    }

  do_color (p, sgr);
  fputs (pfx, p->file);

  HANDLE_ERROR
    (
      p,
      e = vfprintf (p->file, fmt, ap),
      e < 0
    );

  p->presep = "";
  p->postsep = "";
  p->presz = 0;
  p->postsz = 0;
  p->nc = 0;
  p->sgr = NULL;
  p->sgr_decor = NULL;

  do_color (p, &sgr0);

  HANDLE_ERROR
    (
      p,
      e = putc ('\n', p->file),
      e == EOF
    );
}

#define DEF_SINGLE_WRAP(name, pfx)   \
  void \
  putter_single_ ## name (struct putter *p, const char *fmt, ...) \
  { \
    va_list ap; \
    va_start (ap, fmt); \
    vsingle (p, &sgr_ ## name, (pfx), fmt, ap); \
    va_end (ap); \
  }

DEF_SINGLE_WRAP (esc, ": ")
DEF_SINGLE_WRAP (delay, "@ ")
DEF_SINGLE_WRAP (label, "& ")
DEF_SINGLE_WRAP (desc, "\" ")
