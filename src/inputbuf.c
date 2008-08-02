/* inputbuf.c */

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

#include <errno.h>
#include <stdlib.h>

#include "inputbuf.h"
#include "ringbuf.h"

struct inputbuf
{
  FILE *file;
  int saving;
  size_t count;
  size_t saved_count;
  struct ringbuf *rb;
  struct ringbuf_reader *reader;
  int err;
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
  ret->saving = 0;
  ret->count = 0;
  ret->err = 0;
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
inputbuf_io_error (struct inputbuf *ib)
{
  return ib->err;
}

int
inputbuf_get (struct inputbuf *ib)
{
  int c;
  if (ib->saving)
    {
      c = ringbuf_reader_get (ib->reader);
      if (c == EOF && ringbuf_space_avail (ib->rb))
        {
          errno = 0;
          c = getc (ib->file);
          if (c == EOF)
            ib->err = errno;
          else
            ringbuf_put (ib->rb, c);
        }
      if (c != EOF)
        ++ib->saved_count;
    }
  else
    {
      c = ringbuf_get (ib->rb);
      if (c == EOF)
        {
          errno = 0;
          c = getc (ib->file);
        }
      if (c == EOF)
        ib->err = errno;
      else
        ++ib->count;
    }
  return c;
}

int
inputbuf_saving (struct inputbuf *ib)
{
  if (ib->saving)
    return 1;
  ib->saving = 1;
  ib->saved_count = 0;
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
  ib->count += ib->saved_count;
  return 0;
}

size_t
inputbuf_get_count (struct inputbuf *ib)
{
  return ib->count;
}

void
inputbuf_reset_count (struct inputbuf *ib)
{
  ib->count = 0;
  ib->saved_count = 0;
}

int
inputbuf_avail (struct inputbuf *ib)
{
  return ib->saving ? !ringbuf_reader_at_end (ib->reader)
    : !ringbuf_is_empty (ib->rb);
}

