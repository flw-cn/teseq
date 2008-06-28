/* buffer.h: input read-ahead ring buffer implementation. */

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

struct ringbuf;

struct ringbuf *ringbuf_new (size_t);
void ringbuf_delete (struct ringbuf *);
size_t ringbuf_space_avail (struct ringbuf *rb);
int ringbuf_putmem (struct ringbuf *rb, const char *mem, size_t memsz);
int ringbuf_put (struct ringbuf *, unsigned char);
int ringbuf_get (struct ringbuf *);

/* buffer iterator. */
struct ringbuf_reader;

struct ringbuf_reader *ringbuf_reader_new (struct ringbuf *);
void ringbuf_reader_delete (struct ringbuf_reader *);
int ringbuf_reader_get (struct ringbuf_reader *);
void ringbuf_reader_consume (struct ringbuf_reader *);

/* vim:set sts=8 ts=8 sw=8 noet: */
