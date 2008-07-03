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

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inputbuf.h"
#include "csi.h"
#include "sgr.h"

#define DEFAULT_LINE_MAX	78

#define CONTROL(c)	((unsigned char)((c) - 0x40))
#define C_ESC		CONTROL('[')

#define GET_COLUMN(c)	(((c) & 0xf0) >> 4)
#define IS_FINAL_COLUMN(col)	((col) >= 4 && (col) <= 7)
#define IS_FINAL_BYTE(c)	IS_FINAL_COLUMN (GET_COLUMN (c))

#define IS_ECMA_INTERMEDIATE_CHAR(c)	(GET_COLUMN (c) == 2)
#define IS_ECMA_FINAL_CHAR(c)	((c) >= 0x30 || (c) < 0x7f)

/* 0x3a (:) is not actually a private parameter, but since it's not
 * used by any standard we're aware of, except ones that aren't used in
 * practice, we'll consider it private for our purposes. */
#define IS_PRIVATE_PARAM_CHAR(c)	(((c) >= 0x3c && (c) <= 0x3f) \
					 || (c) == 0x3a)

#define DEFAULT_PARAM	((unsigned int)-1)

enum processor_state {
	ST_INIT,
	ST_TEXT,
	ST_CTRL,
	ST_CTRL_NOSEQ
};

struct processor {
	struct inputbuf *ibuf;
	FILE *outf;
	enum processor_state st;
	size_t	             nc; /* Number of characters in current
                                    output line. */
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

struct {
	int	descriptions;
	int	labels;
	int	escapes;
} config;

#define	is_ascii_cntrl(x)	((x) < 0x20)
#define	is_ascii_digit(x)	((x) >= 0x30 && (x) <= 0x39)

#define N_ARY_ELEMS(ary)	(sizeof (ary) / sizeof (ary)[0])

void
print_sgr_param_description (struct processor *p, unsigned int param)
{
	const char *msg = NULL;
	if (param < N_ARY_ELEMS(sgr_param_descriptions))
		msg = sgr_param_descriptions[param];
	if (msg)
		fprintf (p->outf, "\" %s\n", msg);
}

void
print_t416_description (struct processor *p, unsigned char n_params,
			unsigned int *params)
{
	const char *fore_back = "foreground";
	if (params[0] == 48)
		fore_back = "background";
	if (n_params == 3 && params[1] == 5)
		fprintf (p->outf, "\" Set %s color to index %d.\n",
	                 fore_back, params[2]);
	else
		fprintf (p->outf, "\" Set %s color (unknown).\n", fore_back);
}

void
interpret_sgr_params (struct processor *p, unsigned char n_params,
                      unsigned int *params)
{
	unsigned char i;
	if (n_params == 0)
		print_sgr_param_description (p, 0u);
	else if (n_params >= 2 && (params[0] == 48 || params[0] == 38))
		print_t416_description (p, n_params, params);
	else for (i = 0; i != n_params; ++i) {
		unsigned int param = params[i];
		if (param == DEFAULT_PARAM)
			param = 0;
		print_sgr_param_description (p, param);
	}
}

void
print_csi_label (struct processor *p, unsigned int c, int private)
{
	const char **label;
	unsigned int i = c - 0x40;
	if (i < N_ARY_ELEMS(csi_labels)) {
		label = csi_labels[i];
		if (label[0]) {
			const char *privmsg = "";
			if (private)
				privmsg = " (private params)";
			fprintf (p->outf, "& %s: %s%s\n", label[0], label[1],
				 privmsg);
		}
	}
	else
		fputs ("& (private function)\n", p->outf);
}

void
process_csi_sequence (struct processor *p)
{
	int c;
	int e = config.escapes;
	int first_param_char_seen = 0;
	int private_params = 0;
	int last_was_digit = 0;
	unsigned char n_params = 0;
	unsigned int  cur_param;
	unsigned int  params[255];

	if (e) putc (':', p->outf);
	c = inputbuf_get (p->ibuf);
	assert (c == C_ESC);
	if (e) fputs (" Esc", p->outf);
	c = inputbuf_get (p->ibuf);
	assert (c == '[');
	if (e) fprintf (p->outf, " %c", c);
	do {
		c = inputbuf_get (p->ibuf);
		if (!first_param_char_seen && !IS_FINAL_BYTE (c)) {
			first_param_char_seen = 1;
			private_params = IS_PRIVATE_PARAM_CHAR (c);
		}
		if (is_ascii_digit (c)) {
			if (last_was_digit) {
				// XXX: range check here.
				cur_param *= 10;
				cur_param += c - '0';
			}
			else {
				if (e) putc (' ', p->outf);
				cur_param = c - '0';
			}
			last_was_digit = 1;
		}
		else {
			if (e) putc (' ', p->outf);
			if (last_was_digit)
				params[n_params++] = cur_param;
			else
				params[n_params++] = DEFAULT_PARAM;
			last_was_digit = 0;
		}
		if (e) putc (c, p->outf);
	} while (!IS_FINAL_BYTE (c));
	if (e) putc ('\n', p->outf);
	if (config.labels)
		print_csi_label (p, c, private_params);
	if (c == 'm') {
		if (config.descriptions && !private_params)
			interpret_sgr_params (p, n_params, params);
	}
	p->st = ST_INIT;
}

int
read_csi_sequence (struct processor *p)
{
	enum {
		SEQ_INIT,
		SEQ_CSI_PARAM_FIRST_CHAR,
		SEQ_CSI_PARAMETER,
		SEQ_CSI_INTERMEDIATE,
	} state = SEQ_INIT;
	int c, col;
	int private_params = 0;

	inputbuf_saving (p->ibuf);
	while (1) {
		c = inputbuf_get (p->ibuf);
		if (c == EOF) goto noseq;
		col = (c & 0xf0) >> 4;
		switch (state) {
		case SEQ_INIT:
			if (c != '[') goto noseq;
			state = SEQ_CSI_PARAM_FIRST_CHAR;
			break;
		case SEQ_CSI_PARAM_FIRST_CHAR:
			state = SEQ_CSI_PARAMETER;
			private_params = IS_PRIVATE_PARAM_CHAR (c);
		case SEQ_CSI_PARAMETER:
			if (col == 2) {
				state = SEQ_CSI_INTERMEDIATE;
				break;
			}
			else if (col == 3) {
				if (!private_params
				    && IS_PRIVATE_PARAM_CHAR (c))
					goto noseq;
				break;
			}
			/* Fall through */
		case SEQ_CSI_INTERMEDIATE:
			if (IS_FINAL_COLUMN (col)) {
				inputbuf_rewind (p->ibuf);
				return 1;
			}
			else if (col != 2) {
				goto noseq;
			}
		}
	}

	abort ();

noseq:
	inputbuf_rewind (p->ibuf);
	p->st = ST_CTRL_NOSEQ;
	return 0;
}

int
handle_ecma_esc_sequence (struct processor *p)
{
	int i;
	int f;

	/* Esc already given. */
	inputbuf_saving (p->ibuf);
	i = inputbuf_get (p->ibuf);
	if (!IS_ECMA_INTERMEDIATE_CHAR (i))
		goto nothandled;
	f = inputbuf_get (p->ibuf);
	if (!IS_ECMA_FINAL_CHAR (f))
		goto nothandled;

	if (p->nc > 0) putc ('\n', p->outf);
	if (config.escapes)
		fprintf (p->outf, ": Esc %c %c\n", i, f);
	p->nc = 0;
	p->st = ST_INIT;
	return 1;

nothandled:
	inputbuf_rewind (p->ibuf);
	inputbuf_get (p->ibuf); /* Eat the ESC again, assumed by process(). */
	p->st = ST_CTRL_NOSEQ;
	return 0;
}

void
init_state (struct processor *p, unsigned char c)
{
	if (c != '\n' && is_ascii_cntrl (c)) {
		p->nc = 0;
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
		case ST_CTRL_NOSEQ:
			if (is_ascii_cntrl (c)) {
				if (c != C_ESC || p->st == ST_CTRL_NOSEQ) {
					const char *name = control_names[c];
					if (p->nc == 0) {
						putc ('.', p->outf);
						p->nc = 1;
					}
					else if (p->nc + 1 + strlen (name)
					         > DEFAULT_LINE_MAX) {
						putc ('\n', p->outf);
						p->st = ST_INIT;
						continue;
					}
					fprintf (p->outf, " %s", name);
					p->nc += 1 + strlen (name);
					p->st = ST_CTRL;
				}
				else if (handle_ecma_esc_sequence (p))
					; /* handled */
				else if (read_csi_sequence (p)) {
					if (p->nc > 0) putc ('\n', p->outf);
					process_csi_sequence (p);
				}
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
eseq [-LDE] [-o out] [in]\n\
\n\
	-L	Don't print labels.\n\
	-D	Don't print descriptions.\n\
	-E	Don't print escape sequences.\n",
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
	FILE *inf = stdin;
	config.descriptions = 1;
	config.labels = 1;
	config.escapes = 1;
	while ((opt = getopt (argc, argv, ":ho:&D\"LE")) != -1) {
		switch (opt) {
			case 'h':
				usage (EXIT_SUCCESS);
				break;
			case 'o':
				p->outf = must_fopen (optarg, "w");
				break;
			case '"':
			case 'D':
				config.descriptions = 0;
				break;
			case '&':
			case 'L':
				config.labels = 0;
				break;
			case 'E':
				config.escapes = 0;
				break;
			case ':':
				fprintf (stderr,
				         "Option -%c requires an argument.\n\n",
				         optopt);
				usage (EXIT_FAILURE);
				break;
			default:
				if (optopt == ':') {
					config.escapes = 0;
					break;
				}
				fprintf (stderr,
					 "Unrecognized option -%c.\n\n", optopt);
				usage (EXIT_FAILURE);
				break;
		}
	}
	if (argv[optind] != NULL) {
		inf = must_fopen (argv[optind], "r");
	}
	p->ibuf = inputbuf_new (inf, 1024);
	if (!p->ibuf) {
		fputs ("Out of memory.\n", stderr);
		exit (EXIT_FAILURE);
	}
}

int
main (int argc, char **argv)
{
	int c;
	struct processor p = { 0, stdout, ST_INIT, 0 };

	configure_processor (&p, argc, argv);
	while ((c = inputbuf_get (p.ibuf)) != EOF)
		process (&p, c);
	finish (&p);
	return EXIT_SUCCESS;
}

/* vim:set sts=8 sw=8 ts=8 noet: */
