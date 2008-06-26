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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_LINE_MAX	78

enum state {
	ST_INIT,
	ST_TEXT,
	ST_CTRL
};

struct processor {
	FILE *inf;
	FILE *outf;
	enum state st;
	size_t	   nc;	/* Number of characters in current output line. */
};

const char *control_names[] = {
	"NUL", "SOH", "STX", "ETX",
	"EOT", "ENQ", "ACK", "BEL",
	"BS", "HT", "LF", "VT",
	"FF", "CR", "SO", "SI",
	"DLE", "DC1", "DC2", "DC3",
	"DC4", "NAK", "SYN", "ETB",
	"CAN", "EM", "SUB", "ESC",
	"IS4", "IS3", "IS2", "IS1"
};

#define	is_ascii_cntrl(x)	((x) < 0x20)

void
init_state (struct processor *p, unsigned char c)
{
	if (c != '\n' && is_ascii_cntrl (c)) {
		putc ('.', p->outf);
		p->nc = 1;
		p->st = ST_CTRL;
	}
	else {
		putc ('|', p->outf);
		p->nc = 1;
		p->st = ST_TEXT;
	}
}

void
process (struct processor *p, unsigned char c)
{
	int handled = 0;
	while (!handled) {
		switch (p->st) {
		case ST_INIT:
			init_state (p, c);
			continue;
		case ST_TEXT:
			if (c == '\n') {
				fputs ("|.\n", p->outf);
				p->st = ST_INIT;
				/* Handled, don't continue. */
			}
			else if (p->nc == DEFAULT_LINE_MAX
			                  - 2) /* space for "|-" */ {
				fputs ("|-\n-|", p->outf);
				putc (c, p->outf);
				p->nc = 3;	/* "-|" and c */
			}
			else if (is_ascii_cntrl (c)) {
				fputs ("|\n", p->outf);
				p->st = ST_INIT;
				continue;
			}
			else {
				putc (c, p->outf);
				++(p->nc);
			}
			break;
		case ST_CTRL:
			if (is_ascii_cntrl (c)) {
				const char *name = control_names[c];
				if (p->nc + 1 + strlen (name)
				    > DEFAULT_LINE_MAX) {
					putc ('\n', p->outf);
					p->st = ST_INIT;
					continue;
				}
				fprintf (p->outf, " %s", name);
				p->nc += 1 + strlen (name);
			}
			else {
				putc ('\n', p->outf);
				p->st = ST_INIT;
				continue;
			}
			break;
		}
		handled = 1;
	}
}

void
finish (struct processor *p)
{
	if (p->st == ST_TEXT)
		putc ('|', p->outf);
	if (p->st != ST_INIT)
		putc ('\n', p->outf);
}

void
usage (int status)
{
	FILE *f = status == EXIT_SUCCESS? stdout : stderr;
	fputs ("\
eseq -h\n\
eseq [-o out] [in]\n",
		f);
	exit (status);
}

FILE *
must_fopen (const char *fname, const char *mode)
{
	FILE *f;
	if (fname[0] == '-' && fname[1] == '\0') {
		if (strchr (mode, 'w'))
			return stdout;
		else
			return stdin;
	}
	f = fopen (fname, mode);
	if (f) return f;
	fprintf (stderr, "Couldn't open file %s: ", fname);
	perror (NULL);
	exit (EXIT_FAILURE);
}

void
configure_processor (struct processor *p, int argc, char **argv)
{
	int opt;
	while ((opt = getopt (argc, argv, ":ho:")) != -1) {
		switch (opt) {
			case 'h':
				usage (EXIT_SUCCESS);
				break;
			case 'o':
				p->outf = must_fopen (optarg, "w");
				break;
			case ':':
				fprintf (stderr,
				         "Option -%c requires an argument.\n\n",
				         optopt);
				usage (EXIT_FAILURE);
				break;
			default:
				fputs ("Unrecognized option -%c.\n\n",
				       stderr);
				usage (EXIT_FAILURE);
				break;
		}
	}
	if (argv[optind] != NULL) {
		p->inf = must_fopen (argv[optind], "r");
	}
}

int
main (int argc, char **argv)
{
	int c;
	struct processor p = { stdin, stdout, ST_INIT, 0 };

	configure_processor (&p, argc, argv);
	while ((c = getc (p.inf)) != EOF)
		process (&p, c);
	finish (&p);
	return EXIT_SUCCESS;
}

/* vim:set sts=8 sw=8 ts=8 noet: */
