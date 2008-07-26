/* teseq.c: Analysis of terminal controls and escape sequences. */

/*
    Copyright (C) 2008 Micah Cowan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "teseq.h"

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define PUTTER_START_ESC    putter_start (p->putr, ":", "", ": ")

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

struct
{
  int control_hats;
  int descriptions;
  int labels;
  int escapes;
  int extensions;
} config;

#define is_normal_text(x)       ((x) >= 0x20 && (x) < 0x7f)
#define is_ascii_digit(x)       ((x) >= 0x30 && (x) <= 0x39)

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
  if (config.labels)
    putter_single (p->putr, "& %s: %s", acro, name);
}

void
print_csi_label (struct processor *p, struct csi_handler *handler,
                 int private)
{
  if (handler->acro)
    {
      const char *privmsg = "";
      if (private)
        privmsg = " (private params)";
      putter_single (p->putr, "& %s: %s%s", handler->acro, handler->label,
                     privmsg);
    }
}

void
print_c1_label (struct processor *p, unsigned char c)
{
  unsigned char i = c - 0x40;
  const char **label = c1_labels[i];
  if (label[0])
    putter_single (p->putr, "& %s: %s", label[0], label[1]);
}

void
init_csi_params (struct csi_handler *handler, size_t *n_params,
                 unsigned int params[])
{
  if (handler->acro)
    {
      if (*n_params == 0)
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
process_csi_sequence (struct processor *p, struct csi_handler *handler)
{
  int c;
  int e = config.escapes;
  int first_param_char_seen = 0;
  int private_params = 0;
  int last = 0;
  size_t n_params = 0;
  size_t cur_param = 0;
  unsigned int params[255];

  if (e)
    PUTTER_START_ESC;
  
  if (e)
    putter_puts (p->putr, " Esc");
  c = inputbuf_get (p->ibuf);
  assert (c == '[');
  if (e)
    putter_printf (p->putr, " [", c);
  do
    {
      c = inputbuf_get (p->ibuf);
      if (!first_param_char_seen && !IS_CSI_FINAL_CHAR (c))
        {
          first_param_char_seen = 1;
          private_params = IS_PRIVATE_PARAM_CHAR (c);
        }
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
              params[n_params++] = cur_param;
              if (e)
                putter_printf (p->putr, " %d", cur_param);
            }
          else if (! IS_CSI_INTERMEDIATE_CHAR (last))
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
    }
  while (!IS_CSI_FINAL_CHAR (c));
  if (e)
    putter_finish (p->putr, "");
  if (config.labels)
    print_csi_label (p, handler, private_params);
  if (config.descriptions && !private_params && handler->acro && handler->fn)
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
          handler->fn (c, p->putr, n_params, params);
        }
    }
}

/* Determine whether the remaining characters after an initial CSI
   make a valid control sequence; and if so, return information about
   the control function from the final byte. */
struct csi_handler *
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
          private_params = IS_PRIVATE_PARAM_CHAR (c);
        case SEQ_CSI_PARAMETER:
          if (IS_CSI_INTERMEDIATE_COLUMN (col))
            {
              state = SEQ_CSI_INTERMEDIATE;
            }
          else if (col == 3)
            {
              if (!private_params && IS_PRIVATE_PARAM_CHAR (c))
                return NULL;
              break;
            }
          /* Fall through */
        case SEQ_CSI_INTERMEDIATE:
          if (IS_CSI_FINAL_COLUMN (col))
            {
              inputbuf_rewind (p->ibuf);
              if (col < 4 || col >= 7)
                return &csi_no_handler;
              if (intermsz == 0)
                return &csi_handlers[c - 0x40];
              else if (intermsz == 1 && interm == 0x20)
                return &csi_spc_handlers[c - 0x40];
              else
                return &csi_no_handler;
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

#define ISO646(lang)    name = " (ISO646, " lang ")"
#define ISO8859(num)    name = " (ISO 8859-" #num ")"
/* Describe the graphical character set being invoked. */
const char *
get_set_name (int set, int final)
{
  const char *name = "";
  if (GET_COLUMN (final) == 3)
    return " (private)";
  if (set == 4)
    switch (final)
      {
      case 0x40:
        name = " (ISO646/IRV:1983)";
        break;
      case 0x41:
        ISO646 ("British");
        break;
      case 0x42:
        name = " (US-ASCII)";
        break;
      case 0x43:
        name = " (Finnish)";
        break;
      case 0x45:
        name = " (Norwegian/Danish)";
        break;
      case 0x47:
        ISO646 ("Swedish");
        break;
      case 0x48:
        ISO646 ("Swedish Names");
        break;
      case 0x49:
        name = " (Katakana)";
        break;
      case 0x4a:
        ISO646 ("Japanese");
        break;
      case 0x59:
        ISO646 ("Italian");
        break;
      case 0x4c:
        ISO646 ("Portuguese");
        break;
      case 0x5a:
        ISO646 ("Spanish");
        break;
      case 0x4b:
        ISO646 ("German");
        break;
      case 0x60:
        ISO646 ("Norwegian");
        break;
      case 0x66:
        ISO646 ("French");
        break;
      case 0x67:
        ISO646 ("Portuguese");
        break;
      case 0x68:
        ISO646 ("Spanish");
        break;
      case 0x69:
        ISO646 ("Hungarian");
        break;
      case 0x6b:
        name = " (Arabic)";
        break;
      }
  else
    switch (final)
      {
      case 0x41:
        ISO8859 (1);
        break;
      case 0x42:
        ISO8859 (2);
        break;
      case 0x43:
        ISO8859 (3);
        break;
      case 0x44:
        ISO8859 (4);
        break;
      case 0x46:
        name = " (Greek)";
        break;
      case 0x47:
        name = " (Arabic)";
        break;
      case 0x48:
        name = " (Hebrew)";
        break;
      case 0x40:
        name = " (Cyrillic)";
        break;
      case 0x4d:
        ISO8859 (9);
        break;
      case 0x56:
        ISO8859 (10);
        break;
      }

  return name;
}

/* Identify control character set invocation. */
void
print_cxd_info (struct processor *p, int intermediate, int final)
{
  if (intermediate == 0x21)
    {
      maybe_print_label (p, "CZD", "C0-DESIGNATE");
      if (config.descriptions && final == 0x40)
        putter_single (p->putr, "\" Designate C0 Set of ISO-646.");
    }
  else
    {
      maybe_print_label (p, "C1D", "C1-DESIGNATE");
      if (config.descriptions && final == 0x43)
        putter_single (p->putr, "\" Designate C1 Control Set of ISO 6429-1983.");
    }
}

/* Identify graphical character set invocation. */
void
print_gxd_info (struct processor *p, int intermediate, int final)
{
  int designate;
  const char *desig_strs = "Z123";
  int set;

  if (intermediate >= 0x2d && intermediate <= 0x2f)
    {
      set = 6;
      designate = intermediate - 0x2c;
    }
  else if (intermediate != 0x27 && intermediate != 0x2c)
    {
      set = 4;
      designate = intermediate - 0x28;
    }
  else
    return;

  if (config.labels)
    {
      putter_single (p->putr, "& G%cD%d: G%d-DESIGNATE 9%d-SET",
                     desig_strs[designate], set, designate, set);
    }
  if (config.descriptions)
    {
      putter_single (p->putr, "\" Designate 9%d-character set "
                     "%c%s to G%d.",
                     set, final, get_set_name (set, final), designate);
    }
}

/* Identify multibyte graphical character set invocation. */
void
print_gxdm_info (struct processor *p, int i1, int f)
{
  int designate;
  const char *desig_strs = "Z123";
  int set;

  if (f == '\x40' || f == '\x41' || f == '\x42')
    {
      if (i1 == 0)
        {
          designate = 0;
          set = 4;
        }
      else
        return;
    }
  else if (i1 >= 0x2d && i1 <= 0x2f)
    {
      set = 6;
      designate = i1 - 0x2c;
    }
  else if (i1 != 0x27 && i1 != 0x2c)
    {
      set = 4;
      designate = i1 - 0x28;
    }
  else
    return;

  if (config.labels)
    {
      putter_single (p->putr, "& G%cDM%d: G%d-DESIGNATE MULTIBYTE 9%d-SET",
                     desig_strs[designate], set, designate, set);
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
  
  if (config.escapes)
    {
      inputbuf_rewind (p->ibuf);

      PUTTER_START_ESC;
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
  else if (i >= 0x27)
    print_gxd_info (p, i, f);
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
      struct csi_handler *h;
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

  if (config.escapes)
    putter_single (p->putr, ": Esc %c", c);
  if (config.labels)
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
  if (config.escapes)
    putter_single (p->putr, ": Esc %c", c);
  if (config.extensions)
    {
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
  if (config.escapes)
    putter_single (p->putr, ": Esc %c", c);
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
      putter_start (p->putr, ".", "", ".");
    }
  if (IS_CONTROL (c) || c == C_DEL)
    {
      const char *name = "DEL";
      if (c < 0x20)
        name = control_names[c];
      if (config.control_hats)
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
      putter_start (p->putr, "|", "|-", "-|");
      p->st = ST_TEXT;
    }
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
              putter_finish (p->putr, "|");
              p->st = ST_INIT;
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
  FILE *f = status == EXIT_SUCCESS ? stdout : stderr;
  fputs ("\
Usage: teseq [-CLDEx] [in] [out]\n\
   or: teseq -h | --help\n\
   or: teseq -V | --version\n\
Format text with terminal escapes and control sequences for human\n\
consumption.\n\
\n", f);
  fputs ("\
 -h, --help     Display usage information (this message).\n\
 -V, --version  Display version and warrantee.\n\
 -C             Don't print ^X for C0 controls.\n\
 -L             Don't print labels.\n\
 -D             Don't print descriptions.\n\
 -E             Don't print escape sequences.\n\
 -x             Identify control sequences from VT100/Xterm\n\
\n\
Report bugs to micah@cowan.name.\n\
", f);
  exit (status);
}

void
version (void)
{
  puts (PACKAGE_STRING);
  puts ("\
Copyright (C) 2008 Micah Cowan <micah@cowan.name>.\n\
License GPLv3+: GNU GPL version 3 or later \
<http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\
");
  exit (EXIT_SUCCESS);
}

  

FILE *
must_fopen (const char *fname, const char *mode)
{
  FILE *f;
  if (fname[0] == '-' && fname[1] == '\0')
    {
      if (strchr (mode, 'w'))
        return stdout;
      else
        return stdin;
    }
  f = fopen (fname, mode);
  if (f)
    return f;
  fprintf (stderr, "Couldn't open file %s: ", fname);
  perror (NULL);
  exit (EXIT_FAILURE);
}

struct option teseq_opts[] = {
  { "help", 0, NULL, 'h' },
  { "version", 0, NULL, 'V' },
  { 0 }
};

void
configure (struct processor *p, int argc, char **argv)
{
  int opt;
  FILE *inf = stdin;
  FILE *outf = stdout;

  config.control_hats = 1;
  config.descriptions = 1;
  config.labels = 1;
  config.escapes = 1;
  config.extensions = 0;

  while ((opt = getopt_long (argc, argv, ":hVo:C^&D\"LEx", teseq_opts, NULL))
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
          config.control_hats = 0;
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
        case 'x':
          config.extensions = 1;
          break;
        case ':':
          fprintf (stderr, "Option -%c requires an argument.\n\n", optopt);
          usage (EXIT_FAILURE);
          break;
        default:
          if (optopt == ':')
            {
              config.escapes = 0;
              break;
            }
          fprintf (stderr, "Unrecognized option -%c.\n\n", optopt);
          usage (EXIT_FAILURE);
          break;
        }
    }
  if (argv[optind] != NULL)
    {
      inf = must_fopen (argv[optind++], "r");
    }
  if (argv[optind] != NULL)
    {
      outf = must_fopen (argv[optind++], "w");
    }
  setvbuf (inf, NULL, _IONBF, 0);
  setvbuf (outf, NULL, _IOLBF, BUFSIZ);
  p->ibuf = inputbuf_new (inf, 1024);
  p->putr = putter_new (outf);
  if (!p->ibuf || !p->putr)
    {
      fputs ("Out of memory.\n", stderr);
      exit (EXIT_FAILURE);
    }
}

int
main (int argc, char **argv)
{
  int c;
  struct processor p = { 0, 0, ST_INIT };

  configure (&p, argc, argv);
  while ((c = inputbuf_get (p.ibuf)) != EOF)
    process (&p, c);
  finish (&p);
  return EXIT_SUCCESS;
}
