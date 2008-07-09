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
#include "putter.h"
#include "csi.h"
#include "sgr.h"


#define CONTROL(c)	((unsigned char)((c) - 0x40))
#define C_ESC		CONTROL('[')

#define GET_COLUMN(c)	(((c) & 0xf0) >> 4)
#define IS_FINAL_COLUMN(col)	((col) >= 4 && (col) <= 7)
#define IS_FINAL_BYTE(c)	IS_FINAL_COLUMN (GET_COLUMN (c))

#define IS_nF_INTERMEDIATE_CHAR(c)	(GET_COLUMN (c) == 2)
#define IS_nF_FINAL_CHAR(c)	((c) >= 0x30 || (c) < 0x7f)

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
};

struct processor {
	struct inputbuf *ibuf;
	struct putter   *putr;
	enum processor_state st;
	int		print_dot;
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

#define	is_normal_text(x)	((x) >= 0x20 && (x) < 0x7f)
#define	is_ascii_digit(x)	((x) >= 0x30 && (x) <= 0x39)

#define N_ARY_ELEMS(ary)	(sizeof (ary) / sizeof (ary)[0])

void
print_sgr_param_description (struct processor *p, unsigned int param)
{
	const char *msg = NULL;
	if (param < N_ARY_ELEMS(sgr_param_descriptions))
		msg = sgr_param_descriptions[param];
	if (msg) {
		putter_single (p->putr, "\" %s", msg);
	}
}

void
print_t416_description (struct processor *p, unsigned char n_params,
			unsigned int *params)
{
	const char *fore_back = "foreground";
	if (params[0] == 48)
		fore_back = "background";
	if (n_params == 3 && params[1] == 5) {
		putter_single (p->putr, "\" Set %s color to index %d.",
			       fore_back, params[2]);
	}
	else {
		putter_single (p->putr, "\" Set %s color (unknown).",
			       fore_back);
	}
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
			putter_single (p->putr, "& %s: %s%s", label[0],
				       label[1], privmsg);
		}
	}
	else {
		putter_single (p->putr, "%s", "& (private function [CSI])");
	}
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

	if (e)
		putter_start (p->putr, ":", "", ": ");
	if (e) putter_puts (p->putr, " Esc");
	c = inputbuf_get (p->ibuf);
	assert (c == '[');
	if (e) putter_printf (p->putr, " [", c);
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
				cur_param = c - '0';
			}
			last_was_digit = 1;
			continue;
		}
		else {
			if (last_was_digit) {
				params[n_params++] = cur_param;
				last_was_digit = 0;
				if (e) putter_printf (p->putr, " %d", cur_param);
			} else
				params[n_params++] = DEFAULT_PARAM;
			if (e) putter_putc (p->putr, ' ');
		}
		if (e) putter_putc (p->putr, c);
	} while (!IS_FINAL_BYTE (c));
	if (e) putter_finish (p->putr, "");
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
		SEQ_CSI_PARAM_FIRST_CHAR,
		SEQ_CSI_PARAMETER,
		SEQ_CSI_INTERMEDIATE,
	} state = SEQ_CSI_PARAM_FIRST_CHAR;
	int c, col;
	int private_params = 0;

	while (1) {
		c = inputbuf_get (p->ibuf);
		if (c == EOF) goto noseq;
		col = GET_COLUMN (c);
		switch (state) {
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
	return 0;
}

#define ISO646(lang)	name = " (ISO646, " lang ")"
#define ISO8859(num)	name = " (ISO 8859-" #num ")"
const char *
get_set_name (int set, int final)
{
	const char *name = "";
	if (GET_COLUMN (final) == 3)
		return " (private)";
	if (set == 4) switch (final) {
case 0x40: name = " (ISO646/IRV:1983)"; break;
case 0x41: ISO646 ("British"); break;
case 0x42: name = " (US-ASCII)"; break;
case 0x47: ISO646 ("Swedish"); break;
case 0x48: ISO646 ("Swedish Names"); break;
case 0x49: name = " (Katakana)"; break;
case 0x4a: ISO646 ("Japanese"); break;
case 0x59: ISO646 ("Italian"); break;
case 0x4c: ISO646 ("Portuguese"); break;
case 0x5a: ISO646 ("Spanish"); break;
case 0x4b: ISO646 ("German"); break;
case 0x60: ISO646 ("Norwegian"); break;
case 0x66: ISO646 ("French"); break;
case 0x67: ISO646 ("Portuguese 2"); break;
case 0x68: ISO646 ("Spanish 2"); break;
case 0x69: ISO646 ("Hungarian"); break;
case 0x6b: name = " (Arabic)"; break;
	}
	else switch (final) {
case 0x41: ISO8859(1); break;
case 0x42: ISO8859(2); break;
case 0x43: ISO8859(3); break;
case 0x44: ISO8859(4); break;
case 0x46: name = " (Greek)"; break;
case 0x47: name = " (Arabic)"; break;
case 0x48: name = " (Hebrew)"; break;
case 0x40: name = " (Cyrillic)"; break;
case 0x4d: ISO8859(9); break;
case 0x56: ISO8859(10); break;
	}

	return name;
}

void
print_ecma_info (struct processor *p, int intermediate, int final)
{
	int designate;
	const char *desig_strs = "Z123";
	int set;

	if (intermediate >= 0x2d && intermediate <= 0x2f) {
		set = 6;
		designate = intermediate - 0x2c;
	}
	else {
		set = 4;
		designate = intermediate - 0x28;
	}

	if (config.labels) {
		putter_single (p->putr, "& G%cD%d: G%d-DESIGNATE 9%d-SET",
			 desig_strs[designate], set, designate, set);
	}
	if (config.descriptions) {
		putter_single (p->putr, "\" Designate 9%d-character set "
				  "%c%s to G%d.",
			 set, final, get_set_name (set, final), designate);
	}
}

int
handle_nF (struct processor *p, unsigned char i)
{
	int f;

	/* Esc already given. */
	if (!IS_nF_INTERMEDIATE_CHAR (i))
		goto nothandled;
	f = inputbuf_get (p->ibuf);
	if (!IS_nF_FINAL_CHAR (f))
		goto nothandled;

	if (config.escapes) {
		putter_single (p->putr, ": Esc %c %c", i, f);
	}
	print_ecma_info (p, i, f);
	p->st = ST_INIT;
	inputbuf_forget (p->ibuf);
	return 1;

nothandled:
	inputbuf_rewind (p->ibuf);
	return 0;
}

int
handle_c1 (struct processor *p, unsigned char c)
{
	if (c == '[') {
		if (read_csi_sequence (p)) {
			process_csi_sequence (p);
			return 1;
		}
	}
	inputbuf_rewind (p->ibuf);
	return 0;
}

int handle_Fp (struct processor *p, unsigned char c)
{
	inputbuf_forget (p->ibuf);
	if (config.escapes)
		putter_single (p->putr, ": Esc %c", c);
	if (config.labels)
		putter_single (p->putr, "%s", "& (private function [Fp])");
	p->st = ST_INIT;
	return 1;
}

int
handle_escape_sequence (struct processor *p)
{
	int c;

	inputbuf_saving (p->ibuf);

	c = inputbuf_get (p->ibuf);
	if (c == EOF || GET_COLUMN (c) == 0 || GET_COLUMN (c) > 7) {
		inputbuf_rewind (p->ibuf);
		return 0;
	}

	switch (GET_COLUMN (c)) {
	case 2:
		return handle_nF (p, c);
	case 3:
		return handle_Fp (p, c);
	case 4:
	case 5:
		return handle_c1 (p, c);
	default:
		inputbuf_rewind (p->ibuf);
	}

	return 0;
}

int
print_control (struct processor *p, unsigned char c)
{
	if (p->print_dot) {
		p->print_dot = 0;
		putter_start (p->putr, ".", "", ".");
	}
	if (c < 0x20)
		putter_printf (p->putr, " %s", control_names[c]);
	else
		putter_printf (p->putr, " x%02X", (unsigned int)c);
	p->st = ST_CTRL;
	return 0;
}

void
init_state (struct processor *p, unsigned char c)
{
	p->print_dot = 1;
	if (c != '\n' && ! is_normal_text (c)) {
		p->st = ST_CTRL;
	}
	else {
		putter_start (p->putr, "|", "|-", "-|");
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
				putter_finish (p->putr, "|.");
				p->st = ST_INIT;
				/* Handled, don't continue. */
			}
			else if (! is_normal_text (c)) {
				putter_finish (p->putr, "|");
				p->st = ST_INIT;
				continue;
			}
			else {
				putter_putc (p->putr, c);
			}
			break;
		case ST_CTRL:
			if (is_normal_text (c)) {
				putter_finish (p->putr, "");
				p->st = ST_INIT;
				continue;
			}
			else if (c != C_ESC || !handle_escape_sequence (p))
				print_control (p, c);
			break;
		}
		handled = 1;
	}
}

void
finish (struct processor *p)
{
	if (p->st == ST_TEXT)
		putter_finish (p->putr, "|");
	else if (p->st != ST_INIT)
		putter_finish (p->putr, "");
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
	FILE *outf = stdout;
	config.descriptions = 1;
	config.labels = 1;
	config.escapes = 1;
	while ((opt = getopt (argc, argv, ":ho:&D\"LE")) != -1) {
		switch (opt) {
			case 'h':
				usage (EXIT_SUCCESS);
				break;
			case 'o':
				outf = must_fopen (optarg, "w");
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
	setvbuf (inf, NULL, _IONBF, 0);
	setvbuf (outf, NULL, _IOLBF, BUFSIZ);
	p->ibuf = inputbuf_new (inf, 1024);
	p->putr = putter_new (outf);
	if (!p->ibuf || !p->putr) {
		fputs ("Out of memory.\n", stderr);
		exit (EXIT_FAILURE);
	}
}

int
main (int argc, char **argv)
{
	int c;
	struct processor p = { 0, 0, ST_INIT };

	configure_processor (&p, argc, argv);
	while ((c = inputbuf_get (p.ibuf)) != EOF)
		process (&p, c);
	finish (&p);
	return EXIT_SUCCESS;
}

/* vim:set sts=8 sw=8 ts=8 noet: */
