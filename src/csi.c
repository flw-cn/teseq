/* csi.c: acronyms, labels, and descriptions for CSI control sequences. */

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

#include "csi.h"
#include "sgr.h"
#include "modes.h"

static void
csi_do_ich (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 1);
  putter_single (putr, ("\" Shift characters after the cursor to make room "
                        "for %d new character%s."), params[0],
                 params[0] == 1 ? "" : "s");
}

static void
csi_do_cuu (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *dir[] = {"up", "down", "right", "left"};
  if (priv) return;
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor %s %d line%s.",
                 dir[ final - 0x41 ],
                 params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_cnl (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *dir[] = {"down", "up"};
  if (priv) return;
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor to the first column, %d line%s %s.",
                 params[0], params[0] == 1 ? "" : "s", dir[ final - 0x45 ]);
}

static void
csi_do_cha (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor to column %d.", params[0]);
}

static void
csi_do_cup (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 2);
  putter_single (putr, "\" Move the cursor to line %d, column %d.",
                 params[0], params[1]);
}

static void
csi_do_cht (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *hv = (final == 0x59) ? "vertical " : "";
  const char *dir = (final == 0x5A) ? "back" : "forward";
  if (priv) return;
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor %s %d %stab stop%s.",
                 dir, params[0], hv, params[0] == 1 ? "" : "s");
}

static void
csi_do_ed (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  const char *space;
  if (priv) return;
  assert (n_params == 1);
  switch (final)
    {
    case 0x4A:
      space = "screen";
      break;
    case 0x4B:
      space = "line";
      break;
    case 0x4F:
      space = "qualified area";
      break;
    default:
      assert (! "get here");
    }
  
  switch (params[0])
    {
    case 0:
      putter_single (putr, ("\" Clear from the cursor to the end of "
                            "the %s."), space);
      break;
    case 1:
      putter_single (putr, ("\" Clear from the beginning of the %s "
                            "to the cursor."), space);
      break;
    case 2:
      putter_single (putr, "\" Clear the %s.", space);
      break;
    }
}

static void
csi_do_il (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  if (priv) return;
  putter_single (putr, ("\" Shift lines after the cursor to make room "
                        "for %d new line%s."), params[0],
                 params[0] == 1 ? "" : "s");
}

static void
csi_do_dl (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  if (priv) return;
  putter_single (putr, "\" Delete %d line%s, shifting the following lines up.",
                 params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_ef (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  if (priv) return;
  switch (params[0])
    {
    case 0:
      putter_single (putr, "\" Clear from the cursor to the next tab stop.");
      break;
    case 1:
      putter_single (putr, ("\" Clear from the previous tab stop "
                            "to the cursor."));
      break;
    case 2:
      putter_single (putr, ("\" Clear from the previous tab stop to the "
                            "next tab stop."));
      break;
    }
}

static void
csi_do_dch (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  if (priv) return;
  putter_single (putr, ("\" Delete %d character%s, shifting the following "
                        "characters left."),
                 params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_cpr (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 2);
  putter_single (putr, ("\" Report that the cursor is located at line %d, "
                        "column %d"), params[0], params[1]);
}

static void
csi_do_su (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  const char *dir, *unit;
  if (priv) return;
  assert (n_params == 1);
  switch (final)
    {
    case 0x53:
      dir = "up";
      unit = "line";
      break;
    case 0x54:
      dir = "down";
      unit = "line";
      break;
    case 0x41:
      dir = "right";
      unit = "column";
      break;
    case 0x40:
      dir = "left";
      unit = "column";
      break;
    default:
      assert (! "got here");
    }
  
  putter_single (putr, "\" Scroll %s by %d %s%s", dir, params[0], unit,
                 params[0] == 1 ? "" : "s");
}

static void
csi_do_ctc (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  unsigned int *p = params, *pend = params + n_params;
  static const char *messages[] = 
    {
      "Set a horizontal tab stop at the cursor position.",
      "Set a vertical tab stop at the current line.",
      "Clear the horizontal tab stop at the cursor position.",
      "Clear the vertical tab stop at the current line.",
      "Clear all horizontal tab stops in the current line.",
      "Clear all horizontal tab stops.",
      "Clear all vertical tab stops."
    };
  
  if (priv) return;
  for (; p != pend; ++p)
    {
      if (*p < N_ARY_ELEMS (messages))
        putter_single (putr, "\" %s", messages[*p]);
    }
}

static void
csi_do_ech (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  putter_single (putr, "\" Erase %d character%s, starting at the cursor.",
                 params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_da (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  if (priv) return;
  if (params[0] != 0)
    return;
  putter_single (putr, "\" Request terminal identification.");
}

static void
csi_do_vpa (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  putter_single (putr, "\" Move the cursor to line %d.", params[0]);
}

/* Describe private mode sets. Based on information from the
   VT220 Programmer Reference Manual (http://vt100.net/docs/vt220-rm/),
   and the Xterm Control Sequences document at
   http://invisible-island.net/xterm/ctlseqs/ctlseqs.html. */
void
handle_private_mode (struct putter *putr, unsigned int param, int set)
{
  const char *msg = NULL;
  
  switch (param)
    {
    case 1:
      if (set) msg = "\" (DEC) Cursor key mode.";
      else     msg = "\" (DEC) Cursor key mode off.";
      break;
    case 2:
      if (set) msg = ("\" (XTerm) Designate US-ASCII for charater sets G0-G3, "
                      "and set VT100 mode.");
      else     msg = "\" (DEC) Designate VT52 mode.";
      break;
    case 3:
      if (set) msg = "\" (DEC) 132 columns per line.";
      else     msg = "\" (DEC) 80 columns per line.";
      break;
    case 4:
      if (set) msg = "\
\" (DEC) Smooth scrolling: allow no more than 6 lines to be added\n\
\"  to the screen per second.";
      else     msg = "\
\" (DEC) Fast scrolling: lines are added to the screen as fast as possible.";
      break;
    case 5:
      if (set) msg = "\" (DEC) Reverse video (dark on light).";
      else     msg = "\" (DEC) Normal video (light on dark).";
      break;
    case 7:
      if (set) msg = "\" (DEC) Text auto-wrap mode.";
      else     msg = "\" (DEC) Text auto-wrap mode off.";
      break;
    case 9:
      if (set) msg = "\" (XTerm) Send mouse X & Y on button press.";
      else     msg = "\" (XTerm) Don't send mouse X & Y on button press.";
      break;
    case 10:
      if (set) msg = "\" (Rxvt) Show toolbar.";
      else     msg = "\" (Rxvt) Hide toolbar.";
      break;
    case 12:
      if (set) msg = "\" (Att610) Start blinking cursor.";
      else     msg = "\" (Att610) Stop blinkin cursor.";
      break;
    case 25:
      if (set) msg = "\" (DEC) Show cursor.";
      else     msg = "\" (DEC) Hide cursor.";
      break;
    case 30:
      if (set) msg = "\" (Rxvt) Show scrollbar.";
      else     msg = "\" (Rxvt) Don't show scrollbar.";
      break;
    case 40:
      if (set) msg = "\" (Xterm) Allow 80 -> 132 mode.";
      else     msg = "\" (Xterm) Disallow 80 -> 132 mode.";
      break;
    case 41:
      if (set) msg = "\" (Xterm) Activate workaround for more(1) bug.";
      else     msg = "\" (Xterm) Disable workaround for more(1) bug.";
      break;
    case 42:
      if (set) msg = "\" (DEC) National character set mode.";
      else     msg = "\" (DEC) Multinational character set mode.";
      break;
    case 44:
      if (set) msg = "\" (Xterm) Turn on margin bell.";
      else     msg = "\" (Xterm) Turn off margin bell.";
      break;
    case 45:
      if (set) msg = "\" (Xterm) Reverse-wraparound mode.";
      else     msg = "\" (Xterm) Reverse-wraparound mode off.";
      break;
    case 46:
      if (set) msg = "\" (Xterm) Start logging.";
      if (set) msg = "\" (Xterm) Stop logging.";
      break;
    case 47:
      if (set) msg = "\" (Xterm) Use alternate screen buffer.";
      else     msg = "\" (Xterm) Use normal screen buffer.";
      break;
    case 66:
      if (set) msg = "\" (DEC) Application keypad.";
      else     msg = "\" (DEC) Numeric keypad.";
      break;
    case 67:
      if (set) msg = "\" (DEC) Backarrow key sends backspace.";
      else     msg = "\" (DEC) Backarrow key sends delete.";
      break;
    case 1000:
      if (set) msg = ("\" (Xterm) Send mouse X & Y on button press and "
                      "release.");
      else     msg = ("\" (Xterm) Don't send mouse X & Y on button press "
                      "and release.");
      break;
    case 1001:
      if (set) msg = "\" (Xterm) Activate hilite mouse tracking.";
      else     msg = "\" (Xterm) Disable hilite mouse tracking.";
      break;
    case 1002:
      if (set) msg = "\" (Xterm) Activate cell motion mouse tracking.";
      else     msg = "\" (Xterm) Disable cell motion mouse tracking.";
      break;
    case 1003:
      if (set) msg = "\" (Xterm) Activate all motion mouse tracking.";
      else     msg = "\" (Xterm) Disable all motion mouse tracking.";
      break;
    case 1004:
      if (set) msg = "\" (Xterm) Send FocusIn/FocusOut events.";
      else     msg = "\" (Xterm) Don't send FocusIn/FocusOut events.";
      break;
    case 1010:
      if (set) msg = "\" (Rxvt) Scroll to bottom on tty output.";
      else     msg = "\" (Rxvt) Don't scroll to bottom on tty output.";
      break;
    case 1011:
      if (set) msg = "\" (Rxvt) Scroll to bottom on key press.";
      else     msg = "\" (Rxvt) Don't scroll to bottom on key press.";
      break;
    case 1034:
      if (set) msg = "\" (Xterm) Interpret meta key, sets eighth bit.";
      else     msg = "\" (Xterm) Don't interpret meta key.";
      break;
    case 1035:
      if (set) msg = ("\" (Xterm) Enable special modifiers for Alt "
                      "and NumLock keys.");
      else     msg = ("\" (Xterm) Disable special modifiers for Alt "
                      "and NumLock keys.");
      break;
    case 1036:
      if (set) msg = "\" (Xterm) Send ESC when Meta modifies a key.";
      else     msg = "\" (Xterm) Don't send ESC when Meta modifies a key.";
      break;
    case 1037:
      if (set) msg = "\" (Xterm) Send DEL from the editing-keypad Delete key.";
      else     msg = ("\" (Xterm) Send VT220 Remove from the "
                      "editing-keypad Delete key.");
      break;
    case 1039:
      if (set) msg = "\" (Xterm) Send ESC when Alt modifies a key.";
      else     msg = "\" (Xterm) Don't send ESC when Alt modifies a key.";
      break;
    case 1040:
      if (set) msg = "\" (Xterm) Keep selection even if not highlighted.";
      else     msg = ("\" (Xterm) Do not keep selection even if not "
                      "highlighted.");
      break;
    case 1041:
      if (set) msg = "\" (Xterm) Use the CLIPBOARD selection.";
      else     msg = "\" (Xterm) Don't use the CLIPBOARD selection.";
      break;
    case 1042:
      if (set) msg = "\
\" (Xterm) Enable Urgency window manager hint when BEL is received.";
      else     msg = "\
\" (Xterm) Disable Urgency window manager hint when BEL is received.";
      break;
    case 1043:
      if (set) msg = "\
\" (Xterm) Enable raising of the window when BEL is received.";
      else     msg = "\
\" (Xterm) Disable raising of the window when BEL is received.";
      break;
    case 1047:
      if (set) msg = "\" (Xterm) Use the alternate screen buffer.";
      else     msg = "\" (Xterm) Use the normal screen buffer.";
      break;
    case 1048:
      if (set) msg = "\" (Xterm) Save the cursor position.";
      else     msg = "\" (Xterm) Restore the cursor position.";
      break;
    case 1049:
      if (set) msg = "\
\" (Xterm) Save the cursor position and use the alternate screen buffer,\n\
\"  clearing it first.";
      else msg = ("\" (Xterm) Leave the alternate screen buffer and "
                  "restore the cursor.");
      break;
    case 2004:
      if (set) msg = "\" (Xterm) Set bracketed paste mode.";
      else     msg = "\" (Xterm) Reset bracketed paste mode.";
      break;
    }

  if (msg)
    putter_single (putr, "%s", msg);
}

static void
csi_do_sm (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  unsigned int *p, *pend = params + n_params;
  for (p=params; p != pend; ++p)
    {
      struct mode_info *m;
      if (priv == '?')
        {
          handle_private_mode (putr, *p, final == 0x68);
          continue;
        }
      else if (priv)
        return;

      if (*p >= N_ARY_ELEMS (modes))
        continue;
      m = &modes[*p];
      if (m->acro)
        {
          const char *arg = (final == 0x68) ? m->set : m->reset;
          putter_single (putr, "\" %s (%s) -> %s", m->name, m->acro, arg);
        }
    }
}

static void
csi_do_tbc (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  static const char *messages[] = 
    {
      "Clear the horizontal tab stop at the cursor position.",
      "Clear the vertical tab stop at the current line.",
      "Clear all horizontal tab stops in the current line.",
      "Clear all horizontal tab stops.",
      "Clear all vertical tab stops.",
      "Clear all tab stops."
    };

  if (priv) return;
  if (params[0] < N_ARY_ELEMS (messages))
    putter_single (putr, "\" %s", messages[params[0]]);
}

static void
csi_do_mc (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  unsigned int p = *params;
  static const char *messages[] =
    {
      "Initiate transfer to a primary auxiliary device.",
      "Initiate transfer from a primary auxiliary device.",
      "Initiate transfer to a secondary auxiliary device.",
      "Initiate transfer from a secondary auxiliary device.",
      "Stop relay to a primary auxiliary device.",
      "Start relay to a primary auxiliary device.",
      "Stop relay to a secondary auxiliary device.",
      "Start relay to a secondary auxiliary device."
    };

  if (priv == '?')
    {
      const char *msg = NULL;
      
      switch (p)
        {
        case 1:
          msg = "(DEC) Print current line.";
          break;
        case 4:
          msg = "(DEC) Turn off autoprint mode.";
          break;
        case 5:
          msg = "(DEC) Turn on autoprint mode.";
          break;
        }

      putter_single (putr, "\" %s", msg);
      return;
    }
  else if (priv) return;
  if (p < N_ARY_ELEMS (messages))
    {
      putter_single (putr, "\" %s", messages[p]);
    }
}

static void
print_sgr_param_description (struct putter *putr, unsigned int param)
{
  const char *msg = NULL;
  if (configuration.extensions && param >= 90 && param <= 107)
    {
      const char *messages[] =
        {
          "(Xterm) Set foreground color gray.",
          "(Xterm) Set foreground color bright red.",
          "(Xterm) Set foreground color bright green.",
          "(Xterm) Set foreground color bright yellow.",
          "(Xterm) Set foreground color bright blue.",
          "(Xterm) Set foreground color bright magenta.",
          "(Xterm) Set foreground color bright cyan.",
          "(Xterm) Set foreground color bright white.",
          NULL,
          NULL,
          "(Xterm) Set background color gray.",
          "(Xterm) Set background color bright red.",
          "(Xterm) Set background color bright green.",
          "(Xterm) Set background color bright yellow.",
          "(Xterm) Set background color bright blue.",
          "(Xterm) Set background color bright magenta.",
          "(Xterm) Set background color bright cyan.",
          "(Xterm) Set background color bright white.",
        };
      msg = messages[param - 90];
    }
  else if (param < N_ARY_ELEMS (sgr_param_descriptions))
    msg = sgr_param_descriptions[param];
  if (msg)
    {
      putter_single (putr, "\" %s", msg);
    }
  if (configuration.extensions && param == 100)
    {
      putter_single (putr, "\" %s", ("(Rxvt) Set foreground and background "
                                     "color to default."));
    }
}

void
print_t416_description (struct putter *putr, unsigned char n_params,
                        unsigned int *params)
{
  const char *fore_back = "foreground";
  if (params[0] == 48)
    fore_back = "background";
  if (n_params == 3 && params[1] == 5)
    {
      putter_single (putr, "\" Set %s color to index %d.",
                     fore_back, params[2]);
    }
  else
    {
      putter_single (putr, "\" Set %s color (unknown).", fore_back);
    }
}

static void
csi_do_sgr (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  unsigned int *pend = params + n_params;
  unsigned int *param;
  
  if (priv) return;
  assert (n_params > 0);
  if (n_params >= 2 && (params[0] == 48 || params[0] == 38))
    print_t416_description (putr, n_params, params);
  else
    for (param = params; param != pend; ++param)
      {
        print_sgr_param_description (putr, *param);
      }
}

static void
csi_do_dsr (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  unsigned int p = *params;
  static const char *messages[] =
    {
      "Device reports ready.",
      "Device reports ready, send DSR request later.",
      "Device reports ready, will send DSR later.",
      "Device reports error, send DSR request later.",
      "Device reports error, will send DSR later.",
      "DSR requested.",
      "Request cursor position report."
    };
  if (priv) return;
  if (p < N_ARY_ELEMS (messages))
    putter_single (putr, "\" %s", messages[p]);
}

static struct csi_handler csi_no_handler = { NULL, NULL };

static struct csi_handler csi_handlers[] =
  {
    {"ICH", "INSERT CHARACTER", CSI_FUNC_PN, csi_do_ich, 1 },  /* x40 */
    {"CUU", "CURSOR UP", CSI_FUNC_PN, csi_do_cuu, 1 },
    {"CUD", "CURSOR DOWN", CSI_FUNC_PN, csi_do_cuu, 1 },
    {"CUF", "CURSOR RIGHT", CSI_FUNC_PN, csi_do_cuu, 1 },
    {"CUB", "CURSOR LEFT", CSI_FUNC_PN, csi_do_cuu, 1 },
    {"CNL", "CURSOR NEXT LINE", CSI_FUNC_PN, csi_do_cnl, 1 },
    {"CPL", "CURSOR PRECEDING LINE", CSI_FUNC_PN, csi_do_cnl, 1 },
    {"CHA", "CURSOR CHARACTER ABSOLUTE", CSI_FUNC_PN, csi_do_cha, 1 },
    {"CUP", "CURSOR POSITION", CSI_FUNC_PN_PN, csi_do_cup, 1, 1 },   /* x48 */
    {"CHT", "CURSOR FORWARD TABULATION", CSI_FUNC_PN, csi_do_cht, 1 },
    {"ED", "ERASE IN PAGE", CSI_FUNC_PS, csi_do_ed, 0 },
    {"EL", "ERASE IN LINE", CSI_FUNC_PS, csi_do_ed, 0 },
    {"IL", "INSERT LINE", CSI_FUNC_PN, csi_do_il, 1 },
    {"DL", "DELETE LINE", CSI_FUNC_PN, csi_do_dl, 1 },
    {"EF", "ERASE IN FIELD", CSI_FUNC_PS, csi_do_ef, 0 },
    {"EA", "ERASE IN AREA"},
    {"DCH", "DELETE CHARACTER", CSI_FUNC_PN, csi_do_dch, 1 },  /* x50 */
    {"SEE", "SELECT EDITING EXTENT"},
    {"CPR", "ACTIVE POSITION REPORT", CSI_FUNC_PN_PN, csi_do_cpr, 1, 1 },
    {"SU", "SCROLL UP", CSI_FUNC_PN, csi_do_su, 1 },
    {"SD", "SCROLL DOWN", CSI_FUNC_PN, csi_do_su, 1 },
    {"NP", "NEXT PAGE"},
    {"PP", "PRECEDING PAGE"},
    {"CTC", "CURSOR TABULATION CONTROL", CSI_FUNC_PS_ANY, csi_do_ctc, 0 },
    {"ECH", "ERASE CHARACTER", CSI_FUNC_PN, csi_do_ech, 1 },   /* x58 */
    {"CVT", "CURSOR LINE TABULATION", CSI_FUNC_PN, csi_do_cht, 1 },
    {"CBT", "CURSOR BACKWARD TABULATION", CSI_FUNC_PN, csi_do_cht, 1 },
    {"SRS", "START REVERSED STRING"},
    {"PTX", "PARALLEL TEXTS"},
    {"SDS", "START DIRECTED STRING"},
    {"SIMD", "SELECT IMPLICIT MOVEMENT DIRECTION"},
    {NULL, NULL},
    {"HPA", "CHARACTER POSITION ABSOLUTE", CSI_FUNC_PN, csi_do_cha, 1 },
                                                            /* ^ x60 */
    {"HPR", "CHARACTER POSITION FORWARD"},
    {"REP", "REPEAT"},
    {"DA", "DEVICE ATTRIBUTES", CSI_FUNC_PS, csi_do_da, 0 },
    {"VPA", "LINE POSITION ABSOLUTE", CSI_FUNC_PN, csi_do_vpa, 1},
    {"VPR", "LINE POSITION FORWARD"},
    {"HVP", "CHARACTER AND LINE POSITION", CSI_FUNC_PN_PN, csi_do_cup, 1, 1 },
    {"TBC", "TABULATION CLEAR", CSI_FUNC_PS, csi_do_tbc, 0 },
    {"SM", "SET MODE", CSI_FUNC_PS_ANY, csi_do_sm },           /* x68 */
    {"MC", "MEDIA COPY", CSI_FUNC_PS, csi_do_mc, 0 },
    {"HPB", "CHARACTER POSITION BACKWARD"},
    {"VPB", "LINE POSITION BACKWARD"},
    {"RM", "RESET MODE", CSI_FUNC_PS_ANY, csi_do_sm },
    {"SGR", "SELECT GRAPHIC RENDITION", CSI_FUNC_PS_ANY, csi_do_sgr, 0 },
    {"DSR", "DEVICE STATUS REPORT", CSI_FUNC_PS, csi_do_dsr, 0 },
    {"DAQ", "DEFINE AREA QUALIFICATION"}
  };

static struct csi_handler csi_spc_handlers[] = 
  {
    {"SL", "SCROLL LEFT", CSI_FUNC_PN, csi_do_su, 1 },
    {"SR", "SCROLL RIGHT", CSI_FUNC_PN, csi_do_su, 1 },
    {"GSM", "GRAPHIC SIZE MODIFICATION"},
    {"GSS", "GRAPHIC SIZE SELECTION"},
    {"FNT", "FONT SELECTION"},
    {"TSS", "THIN SPACE SPECIFICATION"},
    {"JFY", "JUSTIFY"},
    {"SPI", "SPACING INCREMENT"},
    {"QUAD", "QUAD"},
    {"SSU", "SELECT SIZE UNIT"},
    {"PFS", "PAGE FORMAT SELECTION"},
    {"SHS", "SELECT CHARACTER SPACING"},
    {"SVS", "SELECT LINE SPACING"},
    {"IGS", "IDENTIFY GRAPHIC SUBREPERTOIRE"},
    {NULL, NULL},
    {"IDCS", "IDENTIFY DEVICE CONTROL STRING"},
    {"PPA", "PAGE POSITION ABSOLUTE"},
    {"PPR", "PAGE POSITION FORWARD"},
    {"PPB", "PAGE POSITION BACKWARD"},
    {"SPD", "SELECT PRESENTATION DIRECTIONS"},
    {"DTA", "DIMENSION TEXT AREA"},
    {"SLH", "SET LINE HOME"},
    {"SLL", "SET LINE LIMIT"},
    {"FNK", "FUNCTION KEY"},
    {"SPQR", "SELECT PRINT QUALITY AND RAPIDITY"},
    {"SEF", "SHEET EJECT AND FEED"},
    {"PEC", "PRESENTATION EXPAND OR CONTRACT"},
    {"SSW", "SET SPACE WIDTH"},
    {"SACS", "SET ADDITIONAL CHARACTER SEPARATION"},
    {"SAPV", "SELECT ALTERNATIVE PRESENTATION VARIANTS"},
    {"STAB", "SELECTIVE TABULATION"},
    {"GCC", "GRAPHIC CHARACTER COMBINATION"},
    {"TATE", "TABULATION ALIGNED TRAILING EDGE"},
    {"TALE", "TABULATION ALIGNED LEADING EDGE"},
    {"TAC", "TABULATION ALIGNED CENTRED"},
    {"TCC", "TABULATION CENTRED ON CHARACTER"},
    {"TSR", "TABULATION STOP REMOVE"},
    {"SCO", "SELECT CHARACTER ORIENTATION"},
    {"SRCS", "SET REDUCED CHARACTER SEPARATION"},
    {"SCS", "SET CHARACTER SPACING"},
    {"SLS", "SET LINE SPACING"},
    {NULL, NULL},
    {NULL, NULL},
    {"SCP", "SELECT CHARACTER PATH"},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL}
  };

struct csi_handler *
get_csi_handler (int exts_on, int private_indicator, size_t intermsz,
                 int interm, unsigned char final)
{
  if (final >= 0x70)
    return &csi_no_handler;
  else if (intermsz == 0)
    return &csi_handlers[final - 0x40];
  else if (intermsz == 1 && interm == 0x20)
    return &csi_spc_handlers[final - 0x40];
  else
    return &csi_no_handler;
}
