/* eseq.c: Analysis of terminal controls and escape sequences. */

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

#define DEFAULT_LINE_MAX	78

enum state {
	ST_INIT,
	ST_TEXT,
	ST_CTRL
};

struct processor {
	enum state st;
	size_t	   nc;	/* Number of characters in current output line. */
};

void
process (struct processor *p, unsigned char c)
{
	switch (p->st) {
	case ST_INIT:
		if (c == '\r') {
			p->st = ST_CTRL;
			process (p, c);
		}
		putchar ('|');
		p->st = ST_TEXT;
		p->nc = 1;
		/* falls through */
	case ST_TEXT:
		if (c == '\n') {
			puts ("|.");
			p->st = ST_INIT;
		}
		else if (p->nc == DEFAULT_LINE_MAX
			       - 2) /* space for "|-" */ {
			fputs ("|-\n-|", stdout);
			putchar (c);
			p->nc = 3;	/* "-|" and c */
		}
		else if (c == '\r') {
			fputs ("|\n.", stdout);
			p->st = ST_CTRL;
			process (p, c);
		}
		else {
			putchar (c);
			++(p->nc);
		}
		break;
	case ST_CTRL:
		if (c == '\r') {
			fputs (" CR", stdout);
		}
		else if (c == '\n') {
			fputs (" LF", stdout);
		}
		else {
			putchar ('\n');
			p->st = ST_INIT;
			process (p, c);
		}
		break;
	}
}

void
finish (struct processor *p)
{
	if (p->st == ST_TEXT)
		putchar ('|');
	if (p->st != ST_INIT)
		putchar ('\n');
}

int
main (void)
{
	int c;
	struct processor p = { ST_INIT, 0 };

	while ((c = getchar ()) != EOF)
		process (&p, c);
	finish (&p);
	return 0;
}

/* vim:set sts=8 sw=8 ts=8 noet: */
