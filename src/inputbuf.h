/* inputbuf.h */

#ifndef INPUTBUF_H
#define INPUTBUF_H

#include <stdio.h>

struct inputbuf;

struct inputbuf *inputbuf_new (FILE *, size_t);
void inputbuf_delete (struct inputbuf *);

int inputbuf_get (struct inputbuf *);
int inputbuf_saving (struct inputbuf *);
int inputbuf_rewind (struct inputbuf *);
int inputbuf_forget (struct inputbuf *);

#endif
