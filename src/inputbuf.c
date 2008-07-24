/* inputbuf.c */

/*
    Copyright (C) 2008 Micah Cowan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "teseq.h"

#include <stdlib.h>

#include "inputbuf.h"
#include "ringbuf.h"

struct inputbuf
{
  FILE *file;
  int last;
  int saving;
  struct ringbuf *rb;
  struct ringbuf_reader *reader;
};

struct inputbuf *
inputbuf_new (FILE * f, size_t bufsz)
{
  struct inputbuf *ret = NULL;
  struct ringbuf *rb = NULL;
  struct ringbuf_reader *reader = NULL;

  ret = malloc (sizeof *ret);
  if (!ret)
    goto cleanup;
  rb = ringbuf_new (bufsz);
  if (!rb)
    goto cleanup;
  reader = ringbuf_reader_new (rb);
  if (!reader)
    goto cleanup;

  ret->rb = rb;
  ret->reader = reader;
  ret->file = f;
  ret->last = EOF;
  return ret;

cleanup:
  free (ret);
  free (rb);
  free (reader);
  return NULL;
}

void
inputbuf_delete (struct inputbuf *ib)
{
  ringbuf_delete (ib->rb);
  free (ib);
}

int
inputbuf_get (struct inputbuf *ib)
{
  int c;
  if (ib->saving)
    {
      c = ringbuf_reader_get (ib->reader);
      if (c == EOF)
        {
          c = getc (ib->file);
          if (c != EOF)
            ringbuf_put (ib->rb, c);
        }
    }
  else
    {
      c = ringbuf_get (ib->rb);
      if (c == EOF)
        c = getc (ib->file);
      ib->last = c;
    }
  return c;
}

int
inputbuf_saving (struct inputbuf *ib)
{
  if (ib->saving)
    return 1;
  ib->saving = 1;
  ringbuf_reader_reset (ib->reader);
  return 0;
}

int
inputbuf_rewind (struct inputbuf *ib)
{
  ringbuf_reader_reset (ib->reader);
  ib->saving = 0;
  return 0;
}

int
inputbuf_forget (struct inputbuf *ib)
{
  if (!ib->saving)
    return 1;
  ringbuf_reader_consume (ib->reader);
  ib->saving = 0;
  return 0;
}
