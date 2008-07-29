/* ringbuf.c: input read-ahead ring buffer implementation. */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ringbuf.h"

#define ADVANCE_CURSOR(rb, c) \
        do { \
                ++(c); \
                if ((c) == (rb)->buf + (rb)->size) \
                        (c) = (rb)->buf; \
        } while (0)

#define BACKUP_CURSOR(rb, c) \
        do { \
                if ((c) == (rb)->buf) \
                        (c) += ((rb)->size - 1u); \
                else \
                        --(c); \
        } while (0)

struct ringbuf
{
  size_t size;
  unsigned char *buf;
  unsigned char *start;
  unsigned char *end;
  int full;
};

struct ringbuf *
ringbuf_new (size_t bufsz)
{
  unsigned char *buf = malloc (bufsz);
  struct ringbuf *newbuf;
  if (!buf)
    return NULL;
  newbuf = malloc (sizeof *newbuf);
  if (!newbuf)
    {
      free (buf);
      return NULL;
    }

  newbuf->size = bufsz;
  newbuf->buf = buf;
  newbuf->start = buf;
  newbuf->end = buf;
  newbuf->full = 0;

  return newbuf;
}

void
ringbuf_delete (struct ringbuf *rb)
{
  free (rb->buf);
  free (rb);
}

int
ringbuf_is_empty (struct ringbuf *rb)
{
  return rb->start == rb->end && !rb->full;
}

int
ringbuf_put (struct ringbuf *rb, unsigned char c)
{
  if (rb->full)
    return 1;
  *rb->end++ = c;
  if (rb->end == rb->buf + rb->size)
    rb->end = rb->buf;
  if (rb->start == rb->end)
    rb->full = 1;
  return 0;
}

int
ringbuf_putback (struct ringbuf *rb, unsigned char c)
{
  if (rb->full)
    return 1;
  BACKUP_CURSOR (rb, rb->start);
  *rb->start = c;
  return 0;
}

size_t
ringbuf_space_avail (struct ringbuf * rb)
{
  if (rb->full)
    return 0u;
  if (rb->end >= rb->start)
    return (((rb->buf + rb->size) - rb->end) + (rb->start - rb->buf));
  return rb->start - rb->end;
}

int
ringbuf_putmem (struct ringbuf *rb, const char *mem, size_t memsz)
{
  if (rb->full)
    return 1;
  if (ringbuf_space_avail (rb) < memsz)
    return 1;
  if (rb->end >= rb->start)
    {
      size_t snip = rb->buf + rb->size - rb->end;
      if (snip > memsz)
        snip = memsz;
      memcpy (rb->end, mem, snip);
      memsz -= snip;
      rb->end += snip;
      if (rb->end == rb->buf + rb->size)
        rb->end = rb->buf;
      if (memsz == 0)
        return 0;
      mem += snip;
    }
  memcpy (rb->end, mem, memsz);
  rb->end += memsz;
  if (rb->end == rb->start)
    rb->full = 1;
  return 0;
}

int
ringbuf_get (struct ringbuf *rb)
{
  int ret;
  if (ringbuf_is_empty (rb))
    return EOF;
  rb->full = 0;
  ret = *rb->start;
  ADVANCE_CURSOR (rb, rb->start);
  if (rb->start == rb->buf + rb->size)
    rb->start = rb->buf;
  return ret;
}

void
ringbuf_clear (struct ringbuf *rb)
{
  rb->start = rb->end;
  rb->full = 0;
}


/* buffer iterator. */
struct ringbuf_reader
{
  struct ringbuf *rb;
  unsigned char *cursor;
};

struct ringbuf_reader *
ringbuf_reader_new (struct ringbuf *rb)
{
  struct ringbuf_reader *reader = malloc (sizeof *reader);
  if (!reader)
    return NULL;
  reader->rb = rb;
  ringbuf_reader_reset (reader);
  return reader;
}

void
ringbuf_reader_delete (struct ringbuf_reader *reader)
{
  free (reader);
}

void
ringbuf_reader_reset (struct ringbuf_reader *reader)
{
  if (ringbuf_is_empty (reader->rb))
    reader->cursor = NULL;
  else
    reader->cursor = reader->rb->start;
}

void
ringbuf_reader_to_end (struct ringbuf_reader *reader)
{
  reader->cursor = NULL;
}

int
ringbuf_reader_at_end (struct ringbuf_reader *reader)
{
  return reader->cursor == NULL;
}

int
ringbuf_reader_get (struct ringbuf_reader *reader)
{
  int ret;
  if (reader->cursor == NULL)
    return EOF;
  ret = *reader->cursor;
  ADVANCE_CURSOR (reader->rb, reader->cursor);
  if (reader->cursor == reader->rb->end)
    reader->cursor = NULL;
  return ret;
}

void
ringbuf_reader_consume (struct ringbuf_reader *reader)
{
  if (reader->cursor == NULL)
    {
      reader->rb->start = reader->rb->end;
      reader->rb->full = 0;
    }
  else if (reader->cursor != reader->rb->start)
    {
      reader->rb->start = reader->cursor;
      reader->rb->full = 0;
    }
}
