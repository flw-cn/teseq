/* teseq.c: Analysis of terminal controls and escape sequences. */

/*
    Copyright (C) 2008 Micah Cowan

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
    putter_single (p->putr, "& %s: %s", acro, name);
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
    PUTTER_START_ESC;
  
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

  if (configuration.descriptions && handler->fn
      && (configuration.extensions || !private_params))
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
              return get_csi_handler (configuration.extensions, private_params,
                                      intermsz, interm, c);
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
      if (configuration.descriptions && final == 0x40)
        putter_single (p->putr, "\" Designate C0 Set of ISO-646.");
    }
  else
    {
      maybe_print_label (p, "C1D", "C1-DESIGNATE");
      if (configuration.descriptions && final == 0x43)
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

  if (configuration.labels)
    {
      putter_single (p->putr, "& G%cD%d: G%d-DESIGNATE 9%d-SET",
                     desig_strs[designate], set, designate, set);
    }
  if (configuration.descriptions)
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
  else if (i1 >= 0x28 && i1 != 0x2c)
    {
      set = 4;
      designate = i1 - 0x28;
    }
  else
    return;

  assert (designate >= 0);
  assert (designate < 4);
  if (configuration.labels)
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
  
  if (configuration.escapes)
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
    putter_single (p->putr, ": Esc %c", c);
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
    putter_single (p->putr, ": Esc %c", c);
  if (configuration.extensions)
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
  if (configuration.escapes)
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
      putter_start (p->putr, "|", "|-", "-|");
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
Usage: teseq [-CLDEx] [in] [out]\n\
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
 -I, --no-interactive\n\
                 Don't put the terminal into non-canonical or no-echo\n\
                 mode, and don't try to ensure output lines are finished\n\
                 when a signal is received.\n\
 -b, --buffered  Force teseq to buffer I/O.\n\
 -t, --timings=TIMINGS\n\
                 Read timing info from TIMINGS and emit delay lines.\n\
 -x              Identify control sequences from VT100/Xterm\n", f);
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
Copyright (C) 2008 Micah Cowan <micah@cowan.name>.\n\
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

  if (tcgetattr (fd, &ti) != 0)
    return;
  saved_stty = ti;
  ti.c_lflag &= ~ICANON;
  if (output_tty_p)
    ti.c_lflag &= ~ECHO;
  working_stty = ti;
  input_term_fd = fd;
  tcsetattr (fd, TCSANOW, &ti);
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

#ifdef HAVE_GETOPT_H
struct option teseq_opts[] = {
  { "help", 0, NULL, 'h' },
  { "version", 0, NULL, 'V' },
  { "timings", 1, NULL, 't' },
  { "buffered", 0, NULL, 'b' },
  { "no-interactive", 0, NULL, 'I' },
  { 0 }
};
#endif

void
configure (struct processor *p, int argc, char **argv)
{
  int opt;
  const char *timings_fname = NULL;
  FILE *inf = stdin;
  FILE *outf = stdout;
  int infd;

  configuration.control_hats = 1;
  configuration.descriptions = 1;
  configuration.labels = 1;
  configuration.escapes = 1;
  configuration.extensions = 0;
  configuration.buffered = 0;
  configuration.handle_signals = 1;
  configuration.timings = NULL;

  program_name = argv[0];

  while ((opt = (
#define ACCEPTOPTS      ":hVo:C^&D\"LEt:xbI"
#ifdef HAVE_GETOPT_H
                 getopt_long (argc, argv, ACCEPTOPTS,
                              teseq_opts, NULL)
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
          configuration.extensions = 1;
          break;
        case ':':
          fprintf (stderr, "Option -%c requires an argument.\n\n", optopt);
          usage (EXIT_FAILURE);
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
      fputs ("Out of memory.\n", stderr);
      exit (EXIT_FAILURE);
    }
  putter_set_handler (p->putr, handle_write_error, argv[0]);
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
        putter_single (p->putr, "@ %f", d.time);
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
    fprintf (stderr, "%s: %s: %s\n", argv[0], "read error", strerror (err));
  return EXIT_SUCCESS;
}
