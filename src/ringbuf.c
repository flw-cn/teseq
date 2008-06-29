/* ringbuf.c: input read-ahead ring buffer implementation. */

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

struct ringbuf {
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
	if (!buf) return NULL;
	struct ringbuf *newbuf = malloc (sizeof *newbuf);
	if (!newbuf) {
		free (buf);
		return NULL;
	}

	newbuf->size   = bufsz;
	newbuf->buf    = buf;
	newbuf->start  = buf;
	newbuf->end    = buf;
	newbuf->full   = 0;

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
	if (rb->full) return 1;
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
	if (rb->full) return 1;
	BACKUP_CURSOR(rb, rb->start);
	*rb->start = c;
	return 0;
}

size_t
ringbuf_space_avail (struct ringbuf *rb)
{
	if (rb->full) return 0u;
	if (rb->end >= rb->start)
		return (((rb->buf + rb->size) - rb->end)
			+ (rb->start - rb->buf));
	return rb->start - rb->end;
}

int
ringbuf_putmem (struct ringbuf *rb, const char *mem, size_t memsz)
{
	if (rb->full) return 1;
	if (ringbuf_space_avail (rb) < memsz) return 1;
	if (rb->end >= rb->start) {
		size_t snip = rb->buf + rb->size - rb->end;
		if (snip > memsz) snip = memsz;
		memcpy (rb->end, mem, snip);
		memsz -= snip;
		rb->end += snip;
		if (rb->end == rb->buf + rb->size)
			rb->end = rb->buf;
		if (memsz == 0) return 0;
		mem += snip;
	}
	memcpy (rb->end, mem, memsz);
	rb->end += memsz;
	if (rb->end == rb->start) rb->full = 1;
	return 0;
}

int
ringbuf_get (struct ringbuf *rb)
{
	int ret;
	if (ringbuf_is_empty (rb)) return EOF;
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
struct ringbuf_reader {
	struct ringbuf *rb;
	unsigned char *cursor;
};

struct ringbuf_reader*
ringbuf_reader_new (struct ringbuf *rb)
{
	struct ringbuf_reader *reader = malloc (sizeof *reader);
	if (!reader) return NULL;
	reader->rb = rb;
	if (ringbuf_is_empty (rb))
		reader->cursor = NULL;
	else
		reader->cursor = rb->start;
	return reader;
}

void
ringbuf_reader_delete (struct ringbuf_reader *reader)
{
	free (reader);
}

int
ringbuf_reader_get (struct ringbuf_reader *reader)
{
	int ret;
	if (reader->cursor == NULL)
		return EOF;
	ret = *reader->cursor;
	ADVANCE_CURSOR(reader->rb, reader->cursor);
	if (reader->cursor == reader->rb->end)
		reader->cursor = NULL;
	return ret;
}

void
ringbuf_reader_consume (struct ringbuf_reader *reader)
{
	if (reader->cursor == NULL) {
		reader->rb->start = reader->rb->end;
		reader->rb->full = 0;
	}
	else if (reader->cursor != reader->rb->start) {
		reader->rb->start = reader->cursor;
		reader->rb->full = 0;
	}
}

/* vim:set sts=8 ts=8 sw=8 noet: */
