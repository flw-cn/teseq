/* ringbuf.h: input read-ahead ring buffer implementation. */

#ifndef RINGBUF_H
#define RINGBUF_H

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

#endif
