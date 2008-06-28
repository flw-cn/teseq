/* buffer.c: input read-ahead ring buffer implementation. */

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

#include "ringbuf.h"

struct ringbuf {
	size_t size;
	unsigned char *buf;
	unsigned char *start;
	unsigned char *end;
	unsigned char *cursor;
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
	newbuf->cursor = buf;
	newbuf->full   = 0;

	return newbuf;
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
ringbuf_get (struct ringbuf *rb)
{
	int ret;
	if (rb->start == rb->end && !rb->full) return EOF;
	rb->full = 0;
	ret = *rb->start++;
	if (rb->start == rb->buf + rb->size)
		rb->start = rb->buf;
	return ret;
}

/* vim:set sts=8 ts=8 sw=8 noet: */
