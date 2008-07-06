/* putter.h */

#ifndef PUTTER_H
#define PUTTER_H

#include <stdio.h>

struct putter;

struct putter *putter_new (FILE *);
void putter_delete (struct putter *);
int putter_start (struct putter *, const char *, const char *, const char *);
int putter_finish (struct putter *, const char *);
int putter_putc (struct putter *, unsigned char);
int putter_puts (struct putter *, const char *);
int putter_printf (struct putter *, const char *, ...);

#endif
