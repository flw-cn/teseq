/* teseq.c: Analysis of terminal controls and escape sequences. */

/*
    Copyright (C) 2008,2010,2013 Micah Cowan

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

#include "teseq.h"

#include <assert.h>
#include <errno.h>
#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "inputbuf.h"
#include "putter.h"

/* label/description maps. */
#include "csi.h"
#include "c1.h"


#define CONTROL(c)      ((unsigned char)((c) - 0x40) & 0x7f)
#define UNCONTROL(c)    ((unsigned char)((c) + 0x40) & 0x7f)
#define C_ESC           (CONTROL ('['))
#define C_DEL           (CONTROL ('?'))

#define GET_COLUMN(c)   (((c) & 0xf0) >> 4)
#define IS_CSI_FINAL_COLUMN(col)    ((col) >= 4 && (col) <= 7)
#define IS_CSI_INTERMEDIATE_COLUMN(col) ((col) == 2)
#define IS_CSI_INTERMEDIATE_CHAR(c) \
  IS_CSI_INTERMEDIATE_COLUMN (GET_COLUMN (c))
#define IS_CSI_FINAL_CHAR(c)        IS_CSI_FINAL_COLUMN (GET_COLUMN (c))

#define IS_nF_INTERMEDIATE_CHAR(c)      (GET_COLUMN (c) == 2)
#define IS_nF_FINAL_CHAR(c)             ((c) >= 0x30 && (c) < 0x7f)
#define IS_CONTROL(c)                   (GET_COLUMN (c) <= 1)

/* 0x3a (:) is not actually a private parameter, but since it's not
 * used by any standard we're aware of, except ones that aren't used in
 * practice, we'll consider it private for our purposes. */
#define IS_PRIVATE_PARAM_CHAR(c)        (((c) >= 0x3c && (c) <= 0x3f) \
                                         || (c) == 0x3a)

enum processor_state
{
  ST_INIT,
  ST_TEXT,
  ST_CTRL
};

struct processor
{
  struct inputbuf *ibuf;
  struct putter *putr;
  enum processor_state st;
  int print_dot;
  size_t mark;
  size_t next_mark;
  
};

struct delay
{
  double time;
  size_t chars;
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

static const char default_color_string[] = "|>=36;7,.=31,:=33,&=35,\"=32,@=34";

struct sgr_def    sgr_text, sgr_text_decor, sgr_ctrl, sgr_esc,
                  sgr_label, sgr_desc, sgr_delay;

struct config configuration = { 0 };
const char *program_name;

static struct termios saved_stty;
static struct termios working_stty;
static int input_term_fd = -1;
static int output_tty_p = 0;
static volatile sig_atomic_t signal_pending_p;
static int pending_signal;


#define is_normal_text(x)       ((x) >= 0x20 && (x) < 0x7f)
#define is_ascii_digit(x)       ((x) >= 0x30 && (x) <= 0x39)

/* Handle write error in putter. */
void
handle_write_error (int e, void *arg)
{
  const char *argv0 = arg;

  fprintf (stderr, "%s: %s: %s\n", argv0, "write error", strerror (e));
  exit (e);
}

/* Read a line from a typescript timings file. */
void
delay_read (FILE *f, struct delay *d)
{
  double time;
  unsigned int chars;
  int converted;
  
  converted = fscanf (f, "%lf %u", &time, &chars);
  if (converted != 2)
    {
      d->time = 0.0;
      d->chars = 0;
      configuration.timings = NULL;
    }
  else
    {
      d->time = time;
      d->chars = chars;
    }
}

void
print_esc_char (struct processor *p, unsigned char c)
{
    if (c == C_ESC)
      putter_puts (p->putr, " Esc");
    else if (c == ' ')
      putter_puts (p->putr, " Spc");
    else
      putter_printf (p->putr, " %c", c);
}

void
maybe_print_label (struct processor *p, const char *acro, const char *name)
{
  if (configuration.labels)
    putter_single_label (p->putr, "%s: %s", acro, name);
}

void
print_csi_label (struct processor *p, const struct csi_handler *handler,
                 int private)
{
  if (handler->acro)
    {
      const char *privmsg = "";
      if (private)
        privmsg = " (private params)";
      putter_single_label (p->putr, "%s: %s%s", handler->acro, handler->label,
                           privmsg);
    }
}

void
print_c1_label (struct processor *p, unsigned char c)
{
  unsigned char i = c - 0x40;
  const char **label = c1_labels[i];
  if (label[0])
    putter_single_label (p->putr, "%s: %s", label[0], label[1]);
}

void
init_csi_params (const struct csi_handler *handler, size_t *n_params,
                 unsigned int params[])
{
  if (handler->fn)
    {
      if (*n_params == 0 && handler->default0 != CSI_DEFAULT_NONE)
        params[(*n_params)++] = handler->default0;
      if (*n_params == 1 && CSI_USE_DEFAULT1 (handler->type))
        params[(*n_params)++] = handler->default1;
    }
}

/* Called after read_csi_sequence has determined that we found a valid
   control sequence. Prints the escape sequence line (if configured),
   collects parameters, and invokes a hook to describe the control
   function (if configured). */
void
process_csi_sequence (struct processor *p, const struct csi_handler *handler)
{
  int c;
  int e = configuration.escapes;
  int private_params = 0;
  int last = 0;
  size_t n_params = 0;

  size_t cur_param = 0;
  unsigned int params[255];

  if (e)
    putter_start (p->putr, &sgr_esc, NULL, ":", "", ": ");
  
  if (e)
    putter_puts (p->putr, " Esc");
  c = inputbuf_get (p->ibuf);
  assert (c == '[');
  if (e)
    putter_printf (p->putr, " [", c);
  c = inputbuf_get (p->ibuf);
  if (!IS_CSI_FINAL_CHAR (c))
    {
      if (IS_PRIVATE_PARAM_CHAR (c))
        {
          private_params = c;
        }
    }
  for (;;)
    {
      if (is_ascii_digit (c))
        {
          if (is_ascii_digit (last))
            {
              /* XXX: range check here. */
              cur_param *= 10;
              cur_param += c - '0';
            }
          else
            {
              cur_param = c - '0';
            }
        }
      else
        {
          if (is_ascii_digit (last))
            {
              if (n_params < N_ARY_ELEMS (params))
                params[n_params++] = cur_param;
              if (e)
                putter_printf (p->putr, " %d", cur_param);
            }
          else if ((last != 0 || private_params == 0)
                   && ! IS_CSI_INTERMEDIATE_CHAR (last)
                   && n_params < N_ARY_ELEMS (params))
            {
              int param = CSI_GET_DEFAULT (handler, n_params);
              if (param >= 0)
                {
                  params[n_params] = param;
                  ++n_params;
                }
            }

          if (e)
            print_esc_char (p, c);
        }
      last = c;
      if (IS_CSI_FINAL_CHAR (c)) break;
      c = inputbuf_get (p->ibuf);
    }
  if (e)
    putter_finish (p->putr, "");
  if (configuration.labels)
    print_csi_label (p, handler, private_params);

  if (configuration.descriptions && handler->fn)
    {
      int wrong_num_params = 0;
      init_csi_params (handler, &n_params, params);
      wrong_num_params = ((handler->type == CSI_FUNC_PN
                           || handler->type == CSI_FUNC_PS)
                          && n_params != 1);
      wrong_num_params |= ((handler->type == CSI_FUNC_PN_PN
                            || handler->type == CSI_FUNC_PS_PS)
                           && n_params != 2);
      if (! wrong_num_params)
        {
          handler->fn (c, private_params, p->putr, n_params, params);
        }
    }
}

/* Determine whether the remaining characters after an initial CSI
   make a valid control sequence; and if so, return information about
   the control function from the final byte. */
const struct csi_handler *
read_csi_sequence (struct processor *p)
{
  enum
  {
    SEQ_CSI_PARAM_FIRST_CHAR,
    SEQ_CSI_PARAMETER,
    SEQ_CSI_INTERMEDIATE
  } state = SEQ_CSI_PARAM_FIRST_CHAR;
  int c, col;
  int private_params = 0;
  unsigned char interm = 0;
  size_t intermsz = 0;

  while (1)
    {
      c = inputbuf_get (p->ibuf);
      if (c == EOF)
        return NULL;
      col = GET_COLUMN (c);
      switch (state)
        {
        case SEQ_CSI_PARAM_FIRST_CHAR:
          state = SEQ_CSI_PARAMETER;
          if (IS_PRIVATE_PARAM_CHAR (c))
            private_params = c;
        case SEQ_CSI_PARAMETER:
          if (IS_CSI_INTERMEDIATE_COLUMN (col))
            {
              state = SEQ_CSI_INTERMEDIATE;
            }
          else if (col == 3)
            {
              if (private_params == 0 && IS_PRIVATE_PARAM_CHAR (c))
                return NULL;
              break;
            }
          /* Fall through */
        case SEQ_CSI_INTERMEDIATE:
          if (IS_CSI_FINAL_COLUMN (col))
            {
              inputbuf_rewind (p->ibuf);
              return get_csi_handler (private_params, intermsz, interm, c);
            }
          else if (! IS_CSI_INTERMEDIATE_COLUMN (col))
            {
              return NULL;
            }
          else
            {
              interm = c;
              ++intermsz;
            }
        }
    }

  abort ();

}

/* Table of names of ISO-IR character sets.
   See http://www.itscj.ipsj.or.jp/ISO-IR/overview.htm  */
static const char * const iso_ir_names[] =
  {
    /* 000 */ NULL,
    /* 001 */ NULL,
    /* 002 */ "ISO_646.irv:1973",
    /* 003 */ NULL,
    /* 004 */ "ISO646-GB", /* = "BS_4730" */
    /* 005 */ NULL,
    /* 006 */ "US-ASCII", /* = "ISO646-US", "ISO_646.irv:1991",
                               "ANSI_X3.4-1968" */
    /* 007 */ NULL,
    /* 008 */ NULL, /* 008-1 and 008-2 handled below: "NATS-SEFI" and "NATS-SEFI-ADD" */
    /* 009 */ NULL, /* 009-1 and 009-2 handled below: "NATS-DANO" and "NATS-DANO-ADD" */
    /* 010 */ "ISO646-SE", /* = "ISO646-FI" = "SEN_850200_B" */
    /* 011 */ "ISO646-SE2", /* = "SEN_850200_C" */
    /* 012 */ NULL,
    /* 013 */ "JIS_C6220-1969-JP",
    /* 014 */ "ISO646-JP",/* = "JIS_C6220-1969" */
    /* 015 */ "ISO646-IT",
    /* 016 */ "ISO646-PT",
    /* 017 */ "ISO646-ES",
    /* 018 */ "GREEK7-OLD",
    /* 019 */ "LATIN-GREEK",
    /* 020 */ NULL,
    /* 021 */ "ISO646-DE", /* = "DIN_66003" */
    /* 022 */ NULL,
    /* 023 */ NULL,
    /* 024 */ NULL,
    /* 025 */ "ISO646-FR1", /* = "NF_Z_62-010_1973" */
    /* 026 */ NULL,
    /* 027 */ "LATIN-GREEK-1",
    /* 028 */ NULL,
    /* 029 */ NULL,
    /* 030 */ NULL,
    /* 031 */ "ISO_5428:1976",
    /* 032 */ NULL,
    /* 033 */ NULL,
    /* 034 */ NULL,
    /* 035 */ NULL,
    /* 036 */ NULL,
    /* 037 */ "ISO_5427",
    /* 038 */ "DIN_31624",
    /* 039 */ "ISO_6438", /* = "DIN_31625" */
    /* 040 */ NULL,
    /* 041 */ NULL,
    /* 042 */ "JIS_C6226-1978",
    /* 043 */ NULL,
    /* 044 */ NULL,
    /* 045 */ NULL,
    /* 046 */ NULL,
    /* 047 */ "ISO-IR-47", /* an ISO646 variant */
    /* 048 */ NULL,
    /* 049 */ "INIS",
    /* 050 */ "INIS-8",
    /* 051 */ "INIS-CYRILLIC",
    /* 052 */ NULL,
    /* 053 */ "ISO_5426", /* = "ISO_5426:1980" */
    /* 054 */ "ISO_5427:1981",
    /* 055 */ "ISO_5428", /* = "ISO_5428:1980" */
    /* 056 */ NULL,
    /* 057 */ "ISO646-CN", /* = "GB_1988-80" */
    /* 058 */ "GB_2312-80",
    /* 059 */ "CODAR-U",
    /* 060 */ "ISO646-NO", /* = "NS_4551-1" */
    /* 061 */ "ISO646-NO2", /* = "NS_4551-2" */
    /* 062 */ NULL,
    /* 063 */ NULL,
    /* 064 */ NULL,
    /* 065 */ NULL,
    /* 066 */ NULL,
    /* 067 */ NULL,
    /* 068 */ "APL",
    /* 069 */ "ISO646-FR", /* = "NF_Z_62-010" */
    /* 070 */ "CCITT-VIDEOTEX", /* not an official name */
    /* 071 */ "CCITT-MOSAIC-2", /* not an official name */
    /* 072 */ NULL,
    /* 073 */ NULL,
    /* 074 */ NULL,
    /* 075 */ NULL,
    /* 076 */ NULL,
    /* 077 */ NULL,
    /* 078 */ NULL,
    /* 079 */ NULL,
    /* 080 */ NULL,
    /* 081 */ NULL,
    /* 082 */ NULL,
    /* 083 */ NULL,
    /* 084 */ "ISO646-PT2",
    /* 085 */ "ISO646-ES2",
    /* 086 */ "ISO646-HU", /* = "MSZ_7795-3" */
    /* 087 */ NULL, /* usused: "JIS_C6226-1983" = "JIS_X0208-1983" */
    /* 088 */ "GREEK7",
    /* 089 */ "ARABIC7", /* = "ASMO_449" */
    /* 090 */ "ISO_6937-2", /* = "ISO_6937-2:1983" */
    /* 091 */ "ISO646-JP-OCR-A",
    /* 092 */ "ISO646-JP-OCR-B",
    /* 093 */ "ISO646-JP-OCR-B-EXT", /* not an official name */
    /* 094 */ "ISO646-JP-OCR-HAND", /* not an official name */
    /* 095 */ "ISO646-JP-OCR-HAND-EXT", /* not an official name */
    /* 096 */ "JIS_C6229-1984-OCR-HAND", /* not an official name */
    /* 097 */ NULL,
    /* 098 */ "ISO_2033",
    /* 099 */ "ANSI_X3.110",
    /* 100 */ "ISO-8859-1",
    /* 101 */ "ISO-8859-2",
    /* 102 */ "ISO646-T.61", /* not an official name */
    /* 103 */ "T.61",
    /* 104 */ NULL,
    /* 105 */ NULL,
    /* 106 */ NULL,
    /* 107 */ NULL,
    /* 108 */ NULL,
    /* 109 */ "ISO-8859-3",
    /* 110 */ "ISO-8859-4",
    /* 111 */ "ECMA-CYRILLIC",
    /* 112 */ NULL,
    /* 113 */ NULL,
    /* 114 */ NULL,
    /* 115 */ NULL,
    /* 116 */ NULL,
    /* 117 */ NULL,
    /* 118 */ NULL,
    /* 119 */ NULL,
    /* 120 */ NULL,
    /* 121 */ "ISO646-CA", /* = "CSA_Z243.4-1985-1" */
    /* 122 */ "ISO646-CA2", /* = "CSA_Z243.4-1985-2" */
    /* 123 */ "CSA_Z243.4-1985-EXT", /* not an official name */
    /* 124 */ NULL,
    /* 125 */ NULL,
    /* 126 */ "ISO-8859-7:1987", /* = "ELOT_128" = "ECMA-118" */
    /* 127 */ "ISO-8859-6", /* = "ECMA-114" = "ASMO-708" */
    /* 128 */ "T.101-2", /* not an official name, same as ISO-IR-99 */
    /* 129 */ "T.101-3", /* not an official name */
    /* 130 */ NULL,
    /* 131 */ NULL,
    /* 132 */ NULL,
    /* 133 */ NULL,
    /* 134 */ NULL,
    /* 135 */ NULL,
    /* 136 */ NULL,
    /* 137 */ "CCITT-MOSAIC-1", /* not an official name */
    /* 138 */ "ISO-8859-8:1988", /* = "ECMA-121" */
    /* 139 */ "CSN_369103",
    /* 140 */ NULL,
    /* 141 */ "ISO646-YU",
    /* 142 */ "BSI_IST-2", /* not an official name */
    /* 143 */ "IEC_P27-1",
    /* 144 */ "ISO-8859-5", /* = "ECMA-113:1988" */
    /* 145 */ NULL,
    /* 146 */ "JUS_003", /* not an official name */
    /* 147 */ "JUS_004", /* not an official name */
    /* 148 */ "ISO-8859-9", /* = "ECMA-128" */
    /* 149 */ "KSC_5601", /* = "KS_C_5601-1987" */
    /* 150 */ "GREEK-CCITT",
    /* 151 */ "ISO646-CU", /* = "NC_99-10:81" */
    /* 152 */ "ISO_6937-2-RESIDUAL", /* not an official name */
    /* 153 */ "GOST_19768-74", /* = "ST_SEV_358-88" */
    /* 154 */ "ISO-IR-154",
    /* 155 */ "ISO_10367-BOX",
    /* 156 */ "ISO_6937:1992",
    /* 157 */ "ISO-8859-10",
    /* 158 */ "ISO-IR-158",
    /* 159 */ "JIS_X0212-1990",
    /* 160 */ NULL,
    /* 161 */ NULL,
    /* 162 */ NULL,
    /* 163 */ NULL,
    /* 164 */ "HEBREW-CCITT", /* not an official name */
    /* 165 */ "CHINESE-CCITT", /* not an official name */
    /* 166 */ "TIS-620", /* = "TIS620-2533:1990" */
    /* 167 */ "ARABIC-BULL", /* not an official name */
    /* 168 */ "JIS_X0208-1990",
    /* 169 */ "BLISSYMBOL", /* not an official name */
    /* 170 */ "ISO646-INV",
    /* 171 */ "CNS11643-1:1986",
    /* 172 */ "CNS11643-2:1986",
    /* 173 */ "CCITT-MOSAIC-3", /* not an official name */
    /* 174 */ NULL,
    /* 175 */ NULL,
    /* 176 */ NULL,
    /* 177 */ NULL,
    /* 178 */ NULL,
    /* 179 */ "ISO-8859-13",
    /* 180 */ "TCVN5712:1993", /* = "VSCII-2" */
    /* 181 */ "ISO-IR-181",
    /* 182 */ "LATIN-WELSH", /* not an official name */
    /* 183 */ "CNS11643-3:1992",
    /* 184 */ "CNS11643-4:1992",
    /* 185 */ "CNS11643-5:1992",
    /* 186 */ "CNS11643-6:1992",
    /* 187 */ "CNS11643-7:1992",
    /* 188 */ NULL,
    /* 189 */ NULL,
    /* 190 */ NULL,
    /* 191 */ NULL,
    /* 192 */ NULL,
    /* 193 */ NULL,
    /* 194 */ NULL,
    /* 195 */ NULL,
    /* 196 */ NULL,
    /* 197 */ "ISO-IR-197",
    /* 198 */ "ISO-8859-8",
    /* 199 */ "ISO-8859-14",
    /* 200 */ "CYRILLIC-URALIC", /* not an official name */
    /* 201 */ "CYRILLIC-VOLGAIC", /* not an official name */
    /* 202 */ "KPS_9566-97",
    /* 203 */ "ISO-8859-15",
    /* 204 */ "ISO-8859-1-EURO", /* not an official name */
    /* 205 */ "ISO-8859-4-EURO", /* not an official name */
    /* 206 */ "ISO-8859-13-EURO", /* not an official name */
    /* 207 */ "ISO646-IE", /* = "IS_433:1996" */
    /* 208 */ "IS_434:1997",
    /* 209 */ "ISO-IR-209",
    /* 210 */ NULL,
    /* 211 */ NULL,
    /* 212 */ NULL,
    /* 213 */ NULL,
    /* 214 */ NULL,
    /* 215 */ NULL,
    /* 216 */ NULL,
    /* 217 */ NULL,
    /* 218 */ NULL,
    /* 219 */ NULL,
    /* 220 */ NULL,
    /* 221 */ NULL,
    /* 222 */ NULL,
    /* 223 */ NULL,
    /* 224 */ NULL,
    /* 225 */ NULL,
    /* 226 */ "ISO-8859-16", /* = "SR_14111:1998" */
    /* 227 */ "ISO-8859-7", /* = "ISO-8859-7:2003" */
    /* 228 */ "JIS_X0213-1:2000",
    /* 229 */ "JIS_X0213-2:2000",
    /* 230 */ "TDS-565",
    /* 231 */ "ANSI_Z39.47",
    /* 232 */ "TDS-616", /* = "TDS-616:2003" */
    /* 233 */ "JIS_X0213-1:2004",
    /* 234 */ "SI1311:2002"
  };

/* Return the name of a character set, given its ISO-IR registry number.  */
static const char *
iso_ir_name (float id)
{
  if (id == 8.1)
    return "NATS-SEFI";
  else if (id == 8.2)
    return "NATS-SEFI-ADD";
  else if (id == 9.1)
    return "NATS-DANO";
  else if (id == 9.2)
    return "NATS-DANO-ADD";
  else
    {
      int i = (int) id;
      const char *name = NULL;
      if (i >= 0 && i < sizeof (iso_ir_names) / sizeof (iso_ir_names[0]))
	name = iso_ir_names[i];
      if (name == NULL)
	{
	  static char buf[20];
	  sprintf (buf, "ISO-IR-%d", i);
	  name = buf;
	}
      return name;
    }
}

/* Table of code sets with 94 characters, assigned 1988-10 or before,
   for 3-byte escape sequences. ESC 0x28..0x2B 0x40+XX.
   See http://www.itscj.ipsj.or.jp/ISO-IR/table01.htm */
static float const iso_ir_table1[] =
  {
     2,  4,  6, 8.1,  8.2, 9.1, 9.2,  10,  11,  13,  14,  21,  16,  39, 37, 38,
    53, 54, 25,  55,   57,  27,  47,  49,  31,  15,  17,  18,  19,  50, 51, 59,
    60, 61, 70,  71,  173,  68,  69,  84,  85,  86,  88,  89,  90,  91, 92, 93,
    94, 95, 96,  98,   99, 102, 103, 121, 122, 137, 141, 146, 128, 147
  };

/* Return the name of a character set, given the final byte
   of the 3-byte escape sequence ESC 0x28..0x2B 0x40+XX.
   Return NULL if unknown.  */
static const char *
iso_ir_table1_name (int f)
{
  if (f >= 0x40 && f < 0x40 + sizeof (iso_ir_table1) / sizeof (iso_ir_table1[0]))
    return iso_ir_name (iso_ir_table1[f - 0x40]);
  else
    return NULL;
}

/* Table of code sets with 94 characters, assigned 1988-11 or later,
   for 4-byte escape sequences ESC 0x28..0x2B 0x21 0x40+XX.
   See http://www.itscj.ipsj.or.jp/ISO-IR/table02.htm */
static int const iso_ir_table2[] =
  {
    150, 151, 170, 207, 230, 231, 232
  };

/* Return the name of a character set, given the final byte
   of the 4-byte escape sequence ESC 0x28..0x2B 0x21 0x40+XX.
   Return NULL if unknown.  */
static const char *
iso_ir_table2_name (int f)
{
  if (f >= 0x40 && f < 0x40 + sizeof (iso_ir_table2) / sizeof (iso_ir_table2[0]))
    return iso_ir_name (iso_ir_table2[f - 0x40]);
  else
    return NULL;
}

/* Table of code sets with 96 characters,
   for 3-byte escape sequences ESC 0x2D..0x2F 0x40+XX.
   See http://www.itscj.ipsj.or.jp/ISO-IR/table03.htm */
static int const iso_ir_table3[] =
  {
    111, 100, 101, 109, 110, 123, 126, 127, 138, 139, 142, 143, 144, 148, 152, 153,
    154, 155, 156, 164, 166, 167, 157,  -1, 158, 179, 180, 181, 182, 197, 198, 199,
    200, 201, 203, 204, 205, 206, 226, 208, 209, 227, 234,  -1,  -1,  -1,  -1,  -1,
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 129
  };

/* Return the name of a character set, given the final byte
   of the 3-byte escape sequence ESC 0x2D..0x2F 0x40+XX.
   Return NULL if unknown.  */
static const char *
iso_ir_table3_name (int f)
{
  if (f >= 0x40 && f < 0x40 + sizeof (iso_ir_table3) / sizeof (iso_ir_table3[0]))
    {
      int id = iso_ir_table3[f - 0x40];
      if (id >= 0)
        return iso_ir_name (id);
    }
  return NULL;
}

/* Table of code sets with multi-byte characters, for escape sequences
   ESC 0x24 [0x28] 0x40+XX (the 0x28 can be omitted only for the first three)
   and ESC 0x24 0x29..0x2B 0x40+XX.
   See http://www.itscj.ipsj.or.jp/ISO-IR/table04.htm */
static int const iso_ir_table4[] =
  {
     42,  58, 168, 149, 159, 165, 169, 171, 172, 183, 184, 185, 186, 187, 202, 228,
    229, 233
  };

/* Return the name of a character set, given the final byte
   of the escape sequence ESC 0x24 [0x28] 0x40+XX (the 0x28 can be omitted only
   for the first three) or ESC 0x24 0x29..0x2B 0x40+XX.
   Return NULL if unknown.  */
static const char *
iso_ir_table4_name (int f)
{
  if (f >= 0x40 && f < 0x40 + sizeof (iso_ir_table3) / sizeof (iso_ir_table3[0]))
    return iso_ir_name (iso_ir_table4[f - 0x40]);
  else
    return NULL;
}

/* Table of names of ISO-IR control character sets.
   See http://www.itscj.ipsj.or.jp/ISO-IR/overview.htm  */
static const char * const iso_ir_control_names[] =
  {
    /* 000 */ NULL,
    /* 001 */ "ISO 646",
    /* 002 */ NULL,
    /* 003 */ NULL,
    /* 004 */ NULL,
    /* 005 */ NULL,
    /* 006 */ NULL,
    /* 007 */ "NATS",
    /* 008 */ NULL,
    /* 009 */ NULL,
    /* 010 */ NULL,
    /* 011 */ NULL,
    /* 012 */ NULL,
    /* 013 */ NULL,
    /* 014 */ NULL,
    /* 015 */ NULL,
    /* 016 */ NULL,
    /* 017 */ NULL,
    /* 018 */ NULL,
    /* 019 */ NULL,
    /* 020 */ NULL,
    /* 021 */ NULL,
    /* 022 */ NULL,
    /* 023 */ NULL,
    /* 024 */ NULL,
    /* 025 */ NULL,
    /* 026 */ "ISO-IR-26",
    /* 027 */ NULL,
    /* 028 */ NULL,
    /* 029 */ NULL,
    /* 030 */ NULL,
    /* 031 */ NULL,
    /* 032 */ NULL,
    /* 033 */ NULL,
    /* 034 */ NULL,
    /* 035 */ NULL,
    /* 036 */ "ISO-IR-36",
    /* 037 */ NULL,
    /* 038 */ NULL,
    /* 039 */ NULL,
    /* 040 */ "DIN_31626",
    /* 041 */ NULL,
    /* 042 */ NULL,
    /* 043 */ NULL,
    /* 044 */ NULL,
    /* 045 */ NULL,
    /* 046 */ NULL,
    /* 047 */ NULL,
    /* 048 */ "INIS",
    /* 049 */ NULL,
    /* 050 */ NULL,
    /* 051 */ NULL,
    /* 052 */ NULL,
    /* 053 */ NULL,
    /* 054 */ NULL,
    /* 055 */ NULL,
    /* 056 */ "VIDEOTEX-GB", /* not an official name */
    /* 057 */ NULL,
    /* 058 */ NULL,
    /* 059 */ NULL,
    /* 060 */ NULL,
    /* 061 */ NULL,
    /* 062 */ NULL,
    /* 063 */ NULL,
    /* 064 */ NULL,
    /* 065 */ NULL,
    /* 066 */ NULL,
    /* 067 */ NULL,
    /* 068 */ NULL,
    /* 069 */ NULL,
    /* 070 */ NULL,
    /* 071 */ NULL,
    /* 072 */ NULL,
    /* 073 */ "VIDEOTEX-CCITT", /* not an official name */
    /* 074 */ "JIS_C6225-1979",
    /* 075 */ NULL,
    /* 076 */ NULL,
    /* 077 */ "ISO_6429-1983",
    /* 078 */ NULL,
    /* 079 */ NULL,
    /* 080 */ NULL,
    /* 081 */ NULL,
    /* 082 */ NULL,
    /* 083 */ NULL,
    /* 084 */ NULL,
    /* 085 */ NULL,
    /* 086 */ NULL,
    /* 087 */ NULL,
    /* 088 */ NULL,
    /* 089 */ NULL,
    /* 090 */ NULL,
    /* 091 */ NULL,
    /* 092 */ NULL,
    /* 093 */ NULL,
    /* 094 */ NULL,
    /* 095 */ NULL,
    /* 096 */ NULL,
    /* 097 */ NULL,
    /* 098 */ NULL,
    /* 099 */ NULL,
    /* 100 */ NULL,
    /* 101 */ NULL,
    /* 102 */ NULL,
    /* 103 */ NULL,
    /* 104 */ "ISO_4873",
    /* 105 */ "ISO_4873",
    /* 106 */ "T.61",
    /* 107 */ "T.61",
    /* 108 */ NULL,
    /* 109 */ NULL,
    /* 110 */ NULL,
    /* 111 */ NULL,
    /* 112 */ NULL,
    /* 113 */ NULL,
    /* 114 */ NULL,
    /* 115 */ NULL,
    /* 116 */ NULL,
    /* 117 */ NULL,
    /* 118 */ NULL,
    /* 119 */ NULL,
    /* 120 */ NULL,
    /* 121 */ NULL,
    /* 122 */ NULL,
    /* 123 */ NULL,
    /* 124 */ "ISO_6630-1985",
    /* 125 */ NULL,
    /* 126 */ NULL,
    /* 127 */ NULL,
    /* 128 */ NULL,
    /* 129 */ NULL,
    /* 130 */ "ASMO_662-1985", /* = "ST_SEV_358" */
    /* 131 */ NULL,
    /* 132 */ "T.101-1", /* not an official name */
    /* 133 */ "T.101-1", /* not an official name */
    /* 134 */ "T.101-2", /* not an official name */
    /* 135 */ "T.101-3", /* not an official name */
    /* 136 */ "T.101-3", /* not an official name */
    /* 137 */ NULL,
    /* 138 */ NULL,
    /* 139 */ NULL,
    /* 140 */ "CSN_369102"
  };

/* Table of C0 control character sets,
   for 3-byte escape sequences ESC 0x21 0x40+XX.
   See http://www.itscj.ipsj.or.jp/ISO-IR/table05.htm */
static int const iso_ir_table5[] =
  {
    1, 7, 48, 26, 36, 106, 74, 104, 130, 132, 134, 135, 140
  };

/* Return the name of a C0 control character set, given the final byte of
   the 3-byte escape sequence ESC 0x21 0x40+XX.
   Return NULL if unknown.  */
static const char *
iso_ir_c0_name (int f)
{
  if (f >= 0x40 && f < 0x40 + sizeof (iso_ir_table5) / sizeof (iso_ir_table5[0]))
    {
      int id = iso_ir_table5[f - 0x40];
      if (id >= 0)
        return iso_ir_control_names[id];
    }
  return NULL;
}

/* Table of C1 control character sets,
   for 3-byte escape sequences ESC 0x22 0x40+XX.
   See http://www.itscj.ipsj.or.jp/ISO-IR/table06.htm */
static int const iso_ir_table6[] =
  {
    56, 73, 124, 77, 133, 40, 136, 105, 107
  };

/* Return the name of a C1 control character set, given the final byte of
   the 3-byte escape sequence ESC 0x22 0x40+XX.
   Return NULL if unknown.  */
static const char *
iso_ir_c1_name (int f)
{
  if (f >= 0x40 && f < 0x40 + sizeof (iso_ir_table6) / sizeof (iso_ir_table6[0]))
    {
      int id = iso_ir_table6[f - 0x40];
      if (id >= 0)
        return iso_ir_control_names[id];
    }
  return NULL;
}

/* Identify control character set invocation.
   Escape sequence: ESC 0x21..0x22 FINAL */
void
print_cxd_info (struct processor *p, int intermediate, int final)
{
  if (intermediate == 0x21)
    {
      maybe_print_label (p, "CZD", "C0-DESIGNATE");
      if (configuration.descriptions)
	{
	  const char *name = iso_ir_c0_name (final);
	  if (name != NULL)
	    putter_single_desc (p->putr, "Designate C0 Control Set of %s.",
                                name);
	}
    }
  else
    {
      maybe_print_label (p, "C1D", "C1-DESIGNATE");
      if (configuration.descriptions)
	{
	  const char *name = iso_ir_c1_name (final);
	  if (name != NULL)
	    putter_single_desc (p->putr, "Designate C1 Control Set of %s.",
                                name);
	}
    }
}

/* Identify graphical character set invocation.
   Escape sequence: ESC 0x28..2F [I1] FINAL */
void
print_gxd_info (struct processor *p, int intermediate, int i1, int final)
{
  /* See ISO 2022 = ECMA 035, section 14.3.2.  */
  int designate;
  const char *desig_strs = "Z123";
  int set;

  if (intermediate >= 0x28 && intermediate <= 0x2b)
    {
      set = 4;
      designate = intermediate - 0x28;
    }
  else if (intermediate >= 0x2d && intermediate <= 0x2f)
    {
      set = 6;
      designate = intermediate - 0x2c;
    }
  else
    return;

  if (configuration.labels)
    {
      putter_single_label (p->putr, "G%cD%d: G%d-DESIGNATE 9%d-SET",
                           desig_strs[designate], set, designate, set);
    }
  if (configuration.descriptions)
    {
      const char *designator;
      const char *explanation;

      {
	static char buf[10];
	char *p = buf;

	if (i1 != 0)
	  *p++ = i1;
	*p++ = final;
	*p = '\0';
	designator = buf;
      }

      if (GET_COLUMN (final) == 3)
	explanation = " (private)";
      else
	{
	  static char buf[100];
	  const char *name;

	  if (set == 4)
	    {
	      if (i1 == 0)
		/* ESC 0x28..0x2B FINAL */
		name = iso_ir_table1_name (final);
	      else if (i1 == 0x21)
		/* ESC 0x28..0x2B 0x21 FINAL */
		name = iso_ir_table2_name (final);
	      else
		name = NULL;
	    }
	  else
	    {
	      if (i1 == 0)
		/* ESC 0x2D..0x2F FINAL */
		name = iso_ir_table3_name (final);
	      else
		name = NULL;
	    }
	  if (name != NULL)
	    {
	      sprintf (buf, " (%s)", name);
	      explanation = buf;
	    }
	  else
	    explanation = "";
	}

      putter_single_desc (p->putr, "Designate 9%d-character set "
                          "%s%s to G%d.",
                          set, designator, explanation, designate);
    }
}

/* Identify multibyte graphical character set invocation.
   Escape sequence: ESC 0x24 [I1] FINAL */
void
print_gxdm_info (struct processor *p, int i1, int final)
{
  /* See ISO 2022 = ECMA 035, section 14.3.2.  */
  int designate;
  const char *desig_strs = "Z123";
  int set;

  if (i1 == (final == 0x40 || final == 0x41 || final == 0x42 ? 0 : 0x28))
    {
      set = 4;
      designate = 0;
    }
  else if (i1 >= 0x29 && i1 <= 0x2b)
    {
      set = 4;
      designate = i1 - 0x28;
    }
  else if (i1 >= 0x2d && i1 <= 0x2f)
    {
      set = 6;
      designate = i1 - 0x2c;
    }
  else
    return;

  assert (designate >= 0);
  assert (designate < 4);
  if (configuration.labels)
    {
      putter_single_label (p->putr, "G%cDM%d: G%d-DESIGNATE MULTIBYTE 9%d-SET",
                           desig_strs[designate], set, designate, set);
    }
  if (configuration.descriptions)
    {
      const char *explanation;

      if (GET_COLUMN (final) == 3)
	explanation = " (private)";
      else
	{
	  static char buf[100];
	  const char *name;

	  name = (set == 4 ? iso_ir_table4_name (final) : NULL);
	  if (name != NULL)
	    {
	      sprintf (buf, " (%s)", name);
	      explanation = buf;
	    }
	  else
	    explanation = "";
	}

      putter_single_desc (p->putr, "Designate multibyte 9%d-character set "
                          "%c%s to G%d.",
                          set, final, explanation, designate);
    }
}

/*
  handle_nF: Handles Ecma-35 (nF)-type escape sequences. These
  generally control switching of character-encoding elements, and
  follow the format "Esc I... F", where "I..." is one or more intermediate
  characters in the range 0x20-0x2f, and the final byte "F" is a
  character in the range 0x30-0x7e. A final byte in the range 0x30-0x3f
  indicates a private function (but the type of function is always indicated
  by the first byte to follow the Esc).
*/
int
handle_nF (struct processor *p, unsigned char i)
{
  int i1 = 0;
  int f;
  int c;

  /* Esc already given. */
  f = inputbuf_get (p->ibuf);
  if (IS_nF_INTERMEDIATE_CHAR (f)) 
    {
      i1 = f;
      f = inputbuf_get (p->ibuf);
      c = f;
      while (IS_nF_INTERMEDIATE_CHAR (c))
        c = inputbuf_get (p->ibuf);
      if (c == EOF || IS_CONTROL (c))
        return 0;
    }
  else if (! IS_nF_FINAL_CHAR (f))
    return 0;
  
  if (configuration.escapes)
    {
      inputbuf_rewind (p->ibuf);

      putter_start (p->putr, &sgr_esc, NULL, ":", "", ": ");
      print_esc_char (p, C_ESC);
      do
        {
          c = inputbuf_get (p->ibuf);
          print_esc_char (p, c);
        }
      while (! IS_nF_FINAL_CHAR (c));

      putter_finish (p->putr, "");
    }

  if (! IS_nF_FINAL_CHAR (f))
    return 1;

  if (i == 0x20)
    maybe_print_label (p, "ACS", "ANNOUNCE CODE STRUCTURE");
  else if (i == 0x21 || i == 0x22)
    print_cxd_info (p, i, f);
  else if (i == 0x24 && (i1 == 0 || i1 >= 0x27))
    print_gxdm_info (p, i1, f);
  else if (i >= 0x28)
    print_gxd_info (p, i, i1, f);
  return 1;
}

/*
  handle_c1: format is Esc Fe, where Fe is a single byte in the range
  0x40-0x5f. It indicates a control from the C1 set of Ecma-48 controls.
  In 8-bit Ecma-35-based encodings, these controls may also be specified
  as a single byte in the range 0x80-0x9f; but this representation is not
  currently supported by Teseq.
  
  If CSI (Esc [) is invoked, further processing is done to determine if
  there is a valid control sequence.
*/
int
handle_c1 (struct processor *p, unsigned char c)
{
  if (c == '[')
    {
      const struct csi_handler *h;
      if ((h = read_csi_sequence (p)))
        {
          process_csi_sequence (p, h);
          return 1;
        }
      else
        {
          inputbuf_rewind (p->ibuf);
          inputbuf_get (p->ibuf);       /* Throw away '[' */
        }
    }

  if (configuration.escapes)
    putter_single_esc (p->putr, "Esc %c", c);
  if (configuration.labels)
    print_c1_label (p, c);
  return 1;
}

/*
  handle_Fp: private function escape sequence, in the format "Esc Fp",
  where Fp is a byte in the 0x30-0x3f range.

  Among common uses for this sequence is the "keypad application mode",
  which VT100-style terminals use to change the key sequences generated by
  keys from the keypad; and "save/restore cursor".
*/
int
handle_Fp (struct processor *p, unsigned char c)
{
  if (configuration.escapes)
    putter_single_esc (p->putr, "Esc %c", c);
  switch (c)
    {
    case '7':
      maybe_print_label (p, "DECSC", "SAVE CURSOR");
      break;
    case '8':
      maybe_print_label (p, "DECRC", "RESTORE CURSOR");
      break;
    case '=':
      maybe_print_label (p, "DECKPAM", "KEYPAD APPLICATION MODE");
      break;
    case '>':
      maybe_print_label (p, "DECKPNM", "KEYPAD NORMAL MODE");
      break;
    }
  return 1;
}

/*
  handle_Fs: Standardized single function control, in the format "Esc Fs",
  where Fs is a byte in the 0x60-0x7e range, and designates a control
  function registered with ISO.
*/
int
handle_Fs (struct processor *p, unsigned char c)
{
  if (configuration.escapes)
    putter_single_esc (p->putr, "Esc %c", c);
  switch (c)
    {
    case 0x60:
      maybe_print_label (p, "DMI", "DISABLE MANUAL INPUT");
      break;
    case 0x61:
      maybe_print_label (p, "INT", "INTERRUPT");
      break;
    case 0x62:
      maybe_print_label (p, "EMI", "END OF MEDIUM");
      break;
    case 0x63:
      maybe_print_label (p, "RIS", "RESET TO INITIAL STATE");
      break;
    case 0x64:
      maybe_print_label (p, "CMD", "CODING METHOD DELIMITER");
      break;
    case 0x6e:
      maybe_print_label (p, "LS2", "LOCKING-SHIFT TWO");
      break;
    case 0x6f:
      maybe_print_label (p, "LS3", "LOCKING-SHIFT THREE");
      break;
    case 0x7c:
      maybe_print_label (p, "LS3R", "LOCKING-SHIFT THREE RIGHT ");
      break;
    case 0x7d:
      maybe_print_label (p, "LS2R", "LOCKING-SHIFT TWO RIGHT ");
      break;
    case 0x7e:
      maybe_print_label (p, "LS1R", "LOCKING-SHIFT ONE RIGHT ");
      break;
    }
  return 1;
}

int
handle_escape_sequence (struct processor *p)
{
  int c;
  int handled = 0;

  inputbuf_saving (p->ibuf);

  c = inputbuf_get (p->ibuf);

  if (c != EOF)
    switch (GET_COLUMN (c))
      {
      case 2:
        handled = handle_nF (p, c);
        break;
      case 3:
        handled = handle_Fp (p, c);
        break;
      case 4:
      case 5:
        handled = handle_c1 (p, c);
        break;
      case 6:
      case 7:
        if (c != C_DEL)
          handled = handle_Fs (p, c);
        break;
      }

  if (handled)
    {
      inputbuf_forget (p->ibuf);
      p->print_dot = 1;
    }
  else
    inputbuf_rewind (p->ibuf);

  return handled;
}

int
print_control (struct processor *p, unsigned char c)
{
  if (p->print_dot)
    {
      p->print_dot = 0;
      putter_start (p->putr, &sgr_ctrl, NULL, ".", "", ".");
    }
  if (IS_CONTROL (c) || c == C_DEL)
    {
      const char *name = "DEL";
      if (c < 0x20)
        name = control_names[c];
      if (configuration.control_hats)
        putter_printf (p->putr, " %s/^%c", name, UNCONTROL (c));
      else
        putter_printf (p->putr, " %s", name);
    }
  else
    putter_printf (p->putr, " x%02X", (unsigned int) c);
  p->st = ST_CTRL;
  return 0;
}

void
init_state (struct processor *p, unsigned char c)
{
  p->print_dot = 1;
  if (c != '\n' && !is_normal_text (c))
    {
      p->st = ST_CTRL;
    }
  else
    {
      putter_start (p->putr, &sgr_text, &sgr_text_decor, "|", "|-", "-|");
      p->st = ST_TEXT;
    }
}

/* Finish the current state and return to ST_INIT. */
void
finish_state (struct processor *p)
{
  switch (p->st)
    {
    case ST_TEXT:
      putter_finish (p->putr, "|");
      break;
    case ST_CTRL:
      putter_finish (p->putr, "");
      break;
    case ST_INIT:
      break;
    default:
      assert (!"Can't get here!");
    }
  
  p->st = ST_INIT;
}

void
catchsig (int s)
{
  if (!signal_pending_p)
    {
      pending_signal = s;
      signal_pending_p = 1;
    }
}

void
handle_pending_signal (struct processor *p)
{
  struct sigaction sa;
  if (!signal_pending_p || inputbuf_avail (p->ibuf))
    return;
  
  if (output_tty_p)
    finish_state (p);

  if (input_term_fd != -1)
    tcsetattr (input_term_fd, TCSANOW, &saved_stty);

  sigaction (pending_signal, NULL, &sa);
  sa.sa_handler = SIG_DFL;
  sigaction (pending_signal, &sa, NULL);
  raise (pending_signal);
  sa.sa_handler = catchsig;
  sigaction (pending_signal, &sa, NULL);

  if (input_term_fd != -1)
    tcsetattr (input_term_fd, TCSANOW, &working_stty);

  signal_pending_p = 0;
}

void
process (struct processor *p, unsigned char c)
{
  int handled = 0;
  while (!handled)
    {
      switch (p->st)
        {
        case ST_INIT:
          /* We're not in the middle of processing
             any particular sort of characters. */
          init_state (p, c);
          continue;
        case ST_TEXT:
          if (c == '\n')
            {
              putter_finish (p->putr, "|.");
              p->st = ST_INIT;
              /* Handled, don't continue. */
            }
          else if (!is_normal_text (c))
            {
              finish_state (p);
              continue;
            }
          else
            {
              putter_putc (p->putr, c);
            }
          break;
        case ST_CTRL:
          if (is_normal_text (c))
            {
              finish_state (p);
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
usage (int status)
{
  FILE *f = status == EXIT_SUCCESS ? stdout : stderr;
  fputs ("\
Usage: teseq [-CLDEx] [in [out]]\n\
   or: teseq -h | --help\n\
   or: teseq -V | --version\n\
Format text with terminal escapes and control sequences for human\n\
consumption.\n", f);
  putc ('\n', f);
  fputs ("\
 -h, --help      Display usage information (this message).\n\
 -V, --version   Display version and warrantee.\n", f);
  fputs ("\
 -C              Don't print ^X for C0 controls.\n\
 -D              Don't print descriptions.\n\
 -E              Don't print escape sequences.\n\
 -L              Don't print labels.\n", f);
  fputs ("\
     --color=[WHEN], --colour=[WHEN]\n\
                 Colorize the output. WHEN defaults to 'always'\n\
                 or can be 'never' or 'auto'. See the full documentation.\n\
 -I, --no-interactive\n\
                 Don't put the terminal into non-canonical or no-echo\n\
                 mode, and don't try to ensure output lines are finished\n\
                 when a signal is received.\n\
 -b, --buffered  Force teseq to buffer I/O.\n\
 -t, --timings=TIMINGS\n\
                 Read timing info from TIMINGS and emit delay lines.\n\
 -x              (No effect; accepted for backwards compatibility.)\n", f);
  putc ('\n', f);
  fputs ("\
The GNU Teseq home page is at http://www.gnu.org/software/teseq/.\n\
Report all bugs to " PACKAGE_BUGREPORT "\n\
", f);
  exit (status);
}

void
version (void)
{
  puts (PACKAGE_STRING);
  puts ("\
Copyright (C) 2008,2013 Micah Cowan <micah@cowan.name>.\n\
License GPLv3+: GNU GPL version 3 or later \
<http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\
");
  exit (EXIT_SUCCESS);
}

FILE *
must_fopen (const char *fname, const char *mode, int dash)
{
  FILE *f;
  if (dash && fname[0] == '-' && fname[1] == '\0')
    {
      if (strchr (mode, 'w'))
        return stdout;
      else
        return stdin;
    }
  f = fopen (fname, mode);
  if (f)
    return f;
  fprintf (stderr, "%s: couldn't open file %s: %s\n", program_name,
           fname, strerror (errno));
  exit (EXIT_FAILURE);
}

void
tty_setup (int fd)
{
  struct termios ti;
  int            intr;
  char          *ctrl_name = NULL;

  if (tcgetattr (fd, &ti) != 0)
    return;
  saved_stty = ti;
  intr = ti.c_cc[VINTR];
  ti.c_lflag &= ~ICANON;
  if (output_tty_p)
    ti.c_lflag &= ~ECHO;
  working_stty = ti;
  input_term_fd = fd;
  tcsetattr (fd, TCSANOW, &ti);

  /* Notify the user that they're in non-canonical mode. */

  fprintf (stderr,
           "  Terminal detected. Interactive mode (-I option to disable).\n"
           "  Send the interrupt character to exit.");

  if (IS_CONTROL(intr))
    {
      fprintf (stderr, " (Control-%c)",
               intr + '@');  /* <--- Example: '\003' -> (Control-C) */
    }
  else if (intr == C_DEL)
    {
      fprintf (stderr, " (DEL, or Control-?)");
    }

  fputs ("\n\n", stderr);
}

void
signal_setup (void)
{
  static const int sigs[] =
    {
      SIGINT,
      SIGTERM,
      SIGTSTP,
      SIGTTIN,
      SIGTTOU
    };
  const int *sig, *sige = sigs + N_ARY_ELEMS (sigs);
  struct sigaction sa;
  sigset_t mask;

  sigemptyset (&mask);
  for (sig = sigs; sig != sige; ++sig)
    sigaddset (&mask, *sig);
  
  sa.sa_handler = catchsig;
  sa.sa_mask = mask;
  sa.sa_flags = 0;
  
  for (sig = sigs; sig != sige; ++sig)
    sigaction (*sig, &sa, NULL);
}

void
parse_colors (const char *color_string)
{
  const char *p, *s, *e;
  struct sgr_def *set_me;

  for (p = color_string; p[0] != '\0'; )
    {
      set_me = NULL;
      switch (p[0])
        {
        case '|':
          if (p[1] == '>')
            {
              set_me = &sgr_text;
              ++p;
            }
          else
            set_me = &sgr_text_decor;
          break;
        case '.': set_me = &sgr_ctrl; break;
        case ':': set_me = &sgr_esc; break;
        case '&': set_me = &sgr_label; break;
        case '"': set_me = &sgr_desc; break;
        case '@': set_me = &sgr_delay; break;
        default:
          ; /* Won't set anything, just skip to next one. */
        }
      if (p[1] != '=')
        {
          /* Invalid definition, skip to next one. */
          set_me = NULL;
        }
      if (p[1] == '\0')
        break;
      for (s = e = &p[2]; e[0] != '\0' && e[0] != ','; ++e)
        {
          if (! (e[0] >= 0x30 && e[0] < 0x40))
            {
              /* Parameters can't fall outside this range. Skip
               * assignment. */
              set_me = NULL;
            }
        }
      if ((set_me != NULL) && ((e - s) <= UINT_MAX))
        {
          set_me->sgr = s;
          set_me->len = e - s;
        }
      if (e[0] == '\0')
        p = e;
      else
        p = e + 1; /* Skip comma. */
    }
}

void
color_setup (void)
{
  const char *envstr = getenv("TESEQ_COLORS");

  if (configuration.color != CFG_COLOR_ALWAYS)
    return;

  parse_colors (default_color_string);
  if (envstr)
    parse_colors (envstr);
}

#ifdef HAVE_GETOPT_H
struct option teseq_opts[] = {
  { "help", 0, NULL, 'h' },
  { "version", 0, NULL, 'V' },
  { "timings", 1, NULL, 't' },
  { "buffered", 0, NULL, 'b' },
  { "no-interactive", 0, NULL, 'I' },
  { "color", 2, &configuration.color, CFG_COLOR_SET },
  { "colour", 2, &configuration.color, CFG_COLOR_SET },
  { 0 }
};
#endif

void
configure (struct processor *p, int argc, char **argv)
{
  int opt, which;
  const char *timings_fname = NULL;
  FILE *inf = stdin;
  FILE *outf = stdout;
  int infd;

  configuration.control_hats = 1;
  configuration.descriptions = 1;
  configuration.labels = 1;
  configuration.escapes = 1;
  configuration.buffered = 0;
  configuration.handle_signals = 1;
  configuration.timings = NULL;
  configuration.color = CFG_COLOR_NONE;

  program_name = argv[0];

  while ((opt = (
#define ACCEPTOPTS      ":hVo:C^&D\"LEt:xbI"
#ifdef HAVE_GETOPT_H
                 getopt_long (argc, argv, ACCEPTOPTS,
                              teseq_opts, &which)
#else
                 getopt (argc, argv, ACCEPTOPTS)
#endif
                 ))
         != -1)
    {
      switch (opt)
        {
        case 'h':
          usage (EXIT_SUCCESS);
          break;
        case 'V':
          version ();
          break;
        case '^':
        case 'C':
          configuration.control_hats = 0;
          break;
        case '"':
        case 'D':
          configuration.descriptions = 0;
          break;
        case '&':
        case 'L':
          configuration.labels = 0;
          break;
        case 'E':
          configuration.escapes = 0;
          break;
        case 'I':
          configuration.handle_signals = 0;
          break;
        case 'b':
          configuration.buffered = 1;
          break;
        case 't':
          timings_fname = optarg;
          break;
        case 'x':
          /* Used to control whether we print descriptions of
           * non-ANSI-defined sequences. This option is always on now. */
          break;
        case ':':
          fprintf (stderr, "Option -%c requires an argument.\n\n", optopt);
          usage (EXIT_FAILURE);
          break;
        case 0:
          /* Long-only option. */
          if (configuration.color == CFG_COLOR_SET)
            {
              if (!optarg || !strcasecmp(optarg, "always"))
                configuration.color = CFG_COLOR_ALWAYS;
              else if (!strcasecmp(optarg, "none"))
                configuration.color = CFG_COLOR_NONE;
              else if (!strcasecmp(optarg, "auto"))
                configuration.color = CFG_COLOR_AUTO;
              else
                {
                  fprintf (stderr,
                           "Option --color: Unknown argument ``%s''.\n\n",
                           optarg);
                  usage (EXIT_FAILURE);
                }
            }
          break;
        default:
          if (optopt == ':')
            {
              configuration.escapes = 0;
              break;
            }
          fprintf (stderr, "Unrecognized option -%c.\n\n", optopt);
          usage (EXIT_FAILURE);
          break;
        }
    }
  if (argv[optind] != NULL)
    {
      inf = must_fopen (argv[optind++], "r", 1);
    }
  if (argv[optind] != NULL)
    {
      outf = must_fopen (argv[optind++], "w", 1);
    }
  if (timings_fname != NULL)
    {
      configuration.timings = must_fopen (timings_fname, "r", 0);
    }

  /* Set input/output to unbuffered. */
  infd = fileno (inf);
  if (!configuration.buffered)
    {
      /* Don't unbuffer if input's a plain file. */
      struct stat s;
      int r = fstat (infd, &s);
      
      if (r == -1 || !S_ISREG (s.st_mode))
        {
          setvbuf (inf, NULL, _IONBF, 0);
          setvbuf (outf, NULL, _IONBF, 0);
        }
    }

  output_tty_p = isatty (fileno (outf));

  if (configuration.color != CFG_COLOR_AUTO)
    ; /* Nothing to do. */
  else if (output_tty_p)
    configuration.color = CFG_COLOR_ALWAYS;
  else
    configuration.color = CFG_COLOR_NONE;

  color_setup ();

  if (configuration.handle_signals)
    {
      if (isatty (infd))
        tty_setup (infd);
      signal_setup ();
    }
  
  p->ibuf = inputbuf_new (inf, 1024);
  p->putr = putter_new (outf);
  if (!p->ibuf || !p->putr)
    {
      fprintf (stderr, "%s: Out of memory.\n", program_name);
      exit (EXIT_FAILURE);
    }
  putter_set_handler (p->putr, handle_write_error, (void *)program_name);
}

void
emit_delay (struct processor *p)
{
  static int first = 1;
  size_t count = inputbuf_get_count (p->ibuf);
  finish_state (p);
  do
    {
      /* Why the "next mark"? ...script issues the amount of delay
         that has occurred *before* a read has been attempted, thus
         scriptreplay.pl Actually reads and executes two delays before
         printing the first byte, to keep the remaining delays
         properly synched. So, we throw out the first delay (but
         remember the number-of-bytes field), and execute the second,
         before _we_ process the byte. */
      struct delay d;
      delay_read (configuration.timings, &d);
      p->mark += p->next_mark;
      p->next_mark = d.chars;
      if (first)
        first = 0;
      else
        putter_single_delay (p->putr, "%f", d.time);
    }
  while (configuration.timings && p->mark <= count);

  /* Following couple lines aren't strictly necessary,
     but keep the count/mark from getting huge, and avoid the
     unlikely potential for overflow. */
  p->mark -= count;
  inputbuf_reset_count (p->ibuf);
}

#define SHOULD_EMIT_DELAY(p)    (configuration.timings && \
                                 (p)->mark <= inputbuf_get_count ((p)->ibuf))

int
main (int argc, char **argv)
{
  int c;
  int err;
  struct processor p = { 0, 0, ST_INIT };

  configure (&p, argc, argv);
  /* If we're in timings mode, we need to handle up to the first
     newline without checking the delay, because that's the timestamp
     line from script, and the delays don't start until after that. */
  if (configuration.timings) 
    {
      while ((c = inputbuf_get (p.ibuf)) != EOF)
        {
          process (&p, c);
          if (c == '\n') break;
        }
      inputbuf_reset_count (p.ibuf);
    }
  for (;;)
    {
      if (SHOULD_EMIT_DELAY (&p))
        emit_delay (&p);
      handle_pending_signal (&p);
      c = inputbuf_get (p.ibuf);
      if (c == EOF)
        {
          if (signal_pending_p)
            continue;
          else
            break;
        }
      else
        {
          process (&p, c);
        }
    }
  finish_state (&p);
  if ((err = inputbuf_io_error (p.ibuf)) != 0)
    fprintf (stderr, "%s: %s: %s\n", program_name, "read error", strerror (err));
  return EXIT_SUCCESS;
}
