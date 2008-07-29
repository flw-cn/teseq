/* ringbuf.h: input read-ahead ring buffer implementation. */

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

/* See test-ringbuf.cm for usage. */

#ifndef RINGBUF_H
#define RINGBUF_H

struct ringbuf;

struct ringbuf *ringbuf_new (size_t);
void ringbuf_delete (struct ringbuf *);
int ringbuf_is_empty (struct ringbuf *rb);
size_t ringbuf_space_avail (struct ringbuf *rb);
int ringbuf_putmem (struct ringbuf *rb, const char *mem, size_t memsz);
int ringbuf_put (struct ringbuf *, unsigned char);
int ringbuf_putback (struct ringbuf *, unsigned char);
int ringbuf_get (struct ringbuf *);
void ringbuf_clear (struct ringbuf *);

/* buffer iterator. */
struct ringbuf_reader;

struct ringbuf_reader *ringbuf_reader_new (struct ringbuf *);
void ringbuf_reader_delete (struct ringbuf_reader *);
void ringbuf_reader_reset (struct ringbuf_reader *);
void ringbuf_reader_to_end (struct ringbuf_reader *);
int ringbuf_reader_at_end (struct ringbuf_reader *);
int ringbuf_reader_get (struct ringbuf_reader *);
void ringbuf_reader_consume (struct ringbuf_reader *);

#endif
