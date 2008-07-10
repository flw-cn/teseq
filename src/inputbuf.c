/* inputbuf.c */

/*
   Copyright (C) 2008 Micah Cowan

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
