/* putter.c */

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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "putter.h"

#define DEFAULT_LINE_MAX	78

struct putter {
	FILE  *file;
	size_t nc;
	const char *presep;
	size_t presz;
	const char *postsep;
	size_t postsz;
	size_t linemax;
};

struct putter *
putter_new (FILE *file)
{
	struct putter *p = malloc (sizeof *p);
	p->file = file;
	p->nc = 0;
	p->linemax = DEFAULT_LINE_MAX;
	p->presep = p->postsep = "";
	p->presz = 0;
	p->postsz = 0;
	return p;
}

void putter_delete (struct putter *p)
{
	free (p);
}

static void
ensure_space (struct putter *p, size_t addition)
{
	if (p->nc + addition > p->linemax
	    || p->nc + p->presz == p->linemax) {
		fprintf (p->file, "%s\n%s", p->presep, p->postsep);
		p->nc = p->postsz;
	}
	p->nc += addition;
}

int putter_start (struct putter *p, const char *s,
		  const char *pre, const char *post)
{
	p->presep = pre;
	p->postsep = post;
	p->presz = strlen (pre);
	p->postsz = strlen (post);

	if (p->nc > 0) fputc ('\n', p->file);
	p->nc = strlen (s);
	return fputs (s, p->file);
}

int putter_finish (struct putter *p, const char *s)
{
	p->presep = "";
	p->postsep = "";
	p->presz = 0;
	p->postsz = 0;

	if (p->nc + strlen (s) > p->linemax)
		fprintf (p->file, "%s\n", p->presep);
	p->nc = 0;
	return fprintf (p->file, "%s\n", s);
}

int putter_putc (struct putter *p, unsigned char c)
{
	ensure_space (p, 1);
	return putc (c, p->file);
}
	
int putter_puts (struct putter *p, const char *s)
{
	ensure_space (p, strlen (s));
	return fputs (s, p->file);
}

int putter_printf (struct putter *p, const char *fmt, ...)
{
	int len, ret;
	va_list ap;
	va_list cp;

	va_start (ap, fmt);
	va_copy  (cp, ap);
	len = vsnprintf (NULL, 0, fmt, ap);
	va_end (ap);
	ensure_space (p, len);
	ret = vfprintf (p->file, fmt, cp);
	va_end (cp);
	return ret;
}

/* Combines:
 *	putter_start (p, "", "", "");
 *	putter_printf (p, fmt, ...);
 *	putter_finish (p, ""); */
int putter_single (struct putter *p, const char *fmt, ...)
{
	va_list ap;
	int ret, e;

	va_start (ap, fmt);
	ret = vfprintf (p->file, fmt, ap);
	va_end (ap);
	p->presep = "";
	p->postsep = "";
	p->presz = 0;
	p->postsz = 0;
	p->nc = 0;
	e = putc ('\n', p->file);
	if (e == -1) return -1;

	return ret+1;
}

/* vim:set sts=8 sw=8 ts=8 noet: */
