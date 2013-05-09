/* csi.c: acronyms, labels, and descriptions for CSI control sequences. */

/*
    Copyright (C) 2008,2013 Micah Cowan

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


/*
  Many of the descriptions below were heavily based on descriptions
  found in the ctlseqs.txt file that accompanies the Xterm
  distribution at http://invisible-island.net/xterm/. The following
  rather long set of copyright and permission notices, which apply to all
  accompanying documentation, accompany that program.

Copyright 2002-2007,2008 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

Copyright 1987, 1988  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1987, 1988 by Digital Equipment Corporation, Maynard.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
*/

/* The control function labels in this document are taken verbatim
   from the Ecma-48 standard. The Ecma website at
   http://www.ecma-international.org/publications/index.html says:

   "Ecma Standards and Technical Reports are made available to all
   interested persons or organizations, free of charge and copyright,
   in printed form and, as files in Acrobat(R) PDF format." */
 
#include "teseq.h"

#include <assert.h>
#include <string.h>

#include "csi.h"
#include "sgr.h"
#include "modes.h"

static void
csi_do_ich (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 1);
  putter_single_desc (putr, ("Shift characters after the cursor to make room "
                             "for %u new character%s."), params[0],
                      params[0] == 1 ? "" : "s");
}

static void
csi_do_cuu (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *dir[] = {"up", "down", "right", "left"};
  const char *unit[] = {"line", "character"};
  if (priv) return;
  assert (n_params == 1);
  putter_single_desc (putr, "Move the cursor %s %u %s%s.",
                      dir[ final - 0x41 ],
                      params[0], unit[ (final - 0x41)/2 ],
                      params[0] == 1 ? "" : "s");
}

static void
csi_do_cnl (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *dir[] = {"down", "up"};
  if (priv) return;
  assert (n_params == 1);
  putter_single_desc (putr, ("Move the cursor to the first column,"
                             " %u line%s %s."),
                      params[0], params[0] == 1 ? "" : "s",
                      dir[ final - 0x45 ]);
}

static void
csi_do_cha (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 1);
  putter_single_desc (putr, "Move the cursor to column %u.", params[0]);
}

static void
csi_do_cup (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 2);
  putter_single_desc (putr, "Move the cursor to line %u, column %u.",
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
  putter_single_desc (putr, "Move the cursor %s %u %stab stop%s.",
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
      putter_single_desc (putr, ("Clear from the cursor to the end of "
                                 "the %s."), space);
      break;
    case 1:
      putter_single_desc (putr, ("Clear from the beginning of the %s "
                                 "to the cursor."), space);
      break;
    case 2:
      putter_single_desc (putr, "Clear the %s.", space);
      break;
    }
}

static void
csi_do_il (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  if (priv) return;
  putter_single_desc (putr, ("Shift lines after the cursor to make room "
                             "for %u new line%s."), params[0],
                      params[0] == 1 ? "" : "s");
}

static void
csi_do_dl (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  if (priv) return;
  putter_single_desc (putr, ("Delete %u line%s, shifting the following "
                             "lines up."),
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
      putter_single_desc (putr, "Clear from the cursor to the next tab stop.");
      break;
    case 1:
      putter_single_desc (putr, ("Clear from the previous tab stop "
                                 "to the cursor."));
      break;
    case 2:
      putter_single_desc (putr, ("Clear from the previous tab stop to the "
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
  putter_single_desc (putr, ("Delete %u character%s, shifting the following "
                             "characters left."),
                      params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_cpr (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  assert (n_params == 2);
  putter_single_desc (putr, ("Report that the cursor is located at line %u, "
                             "column %u"), params[0], params[1]);
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
  
  putter_single_desc (putr, "Scroll %s by %u %s%s", dir, params[0], unit,
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
        putter_single_desc (putr, "%s", messages[*p]);
    }
}

static void
csi_do_ech (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  putter_single_desc (putr, "Erase %u character%s, starting at the cursor.",
                      params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_da (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  if (priv) return;
  if (params[0] != 0)
    return;
  putter_single_desc (putr, "Request terminal identification.");
}

static void
csi_do_vpa (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  if (priv) return;
  putter_single_desc (putr, "Move the cursor to line %u.", params[0]);
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
      if (set) msg = "(DEC) Cursor key mode.";
      else     msg = "(DEC) Cursor key mode off.";
      break;
    case 2:
      if (set) msg = ("(XTerm) Designate US-ASCII for charater sets G0-G3, "
                      "and set VT100 mode.");
      else     msg = "(DEC) Designate VT52 mode.";
      break;
    case 3:
      if (set) msg = "(DEC) 132 columns per line.";
      else     msg = "(DEC) 80 columns per line.";
      break;
    case 4:
      if (set) msg = "\
(DEC) Smooth scrolling: allow no more than 6 lines to be added\n\
\"  to the screen per second.";
      else     msg = "\
(DEC) Fast scrolling: lines are added to the screen as fast as possible.";
      break;
    case 5:
      if (set) msg = "(DEC) Reverse video (dark on light).";
      else     msg = "(DEC) Normal video (light on dark).";
      break;
    case 7:
      if (set) msg = "(DEC) Text auto-wrap mode.";
      else     msg = "(DEC) Text auto-wrap mode off.";
      break;
    case 9:
      if (set) msg = "(XTerm) Send mouse X & Y on button press.";
      else     msg = "(XTerm) Don't send mouse X & Y on button press.";
      break;
    case 10:
      if (set) msg = "(Rxvt) Show toolbar.";
      else     msg = "(Rxvt) Hide toolbar.";
      break;
    case 12:
      if (set) msg = "(Att610) Start blinking cursor.";
      else     msg = "(Att610) Stop blinkin cursor.";
      break;
    case 25:
      if (set) msg = "(DEC) Show cursor.";
      else     msg = "(DEC) Hide cursor.";
      break;
    case 30:
      if (set) msg = "(Rxvt) Show scrollbar.";
      else     msg = "(Rxvt) Don't show scrollbar.";
      break;
    case 40:
      if (set) msg = "(Xterm) Allow 80 -> 132 mode.";
      else     msg = "(Xterm) Disallow 80 -> 132 mode.";
      break;
    case 41:
      if (set) msg = "(Xterm) Activate workaround for more(1) bug.";
      else     msg = "(Xterm) Disable workaround for more(1) bug.";
      break;
    case 42:
      if (set) msg = "(DEC) National character set mode.";
      else     msg = "(DEC) Multinational character set mode.";
      break;
    case 44:
      if (set) msg = "(Xterm) Turn on margin bell.";
      else     msg = "(Xterm) Turn off margin bell.";
      break;
    case 45:
      if (set) msg = "(Xterm) Reverse-wraparound mode.";
      else     msg = "(Xterm) Reverse-wraparound mode off.";
      break;
    case 46:
      if (set) msg = "(Xterm) Start logging.";
      if (set) msg = "(Xterm) Stop logging.";
      break;
    case 47:
      if (set) msg = "(Xterm) Use alternate screen buffer.";
      else     msg = "(Xterm) Use normal screen buffer.";
      break;
    case 66:
      if (set) msg = "(DEC) Application keypad.";
      else     msg = "(DEC) Numeric keypad.";
      break;
    case 67:
      if (set) msg = "(DEC) Backarrow key sends backspace.";
      else     msg = "(DEC) Backarrow key sends delete.";
      break;
    case 1000:
      if (set) msg = ("(Xterm) Send mouse X & Y on button press and "
                      "release.");
      else     msg = ("(Xterm) Don't send mouse X & Y on button press "
                      "and release.");
      break;
    case 1001:
      if (set) msg = "(Xterm) Activate hilite mouse tracking.";
      else     msg = "(Xterm) Disable hilite mouse tracking.";
      break;
    case 1002:
      if (set) msg = "(Xterm) Activate cell motion mouse tracking.";
      else     msg = "(Xterm) Disable cell motion mouse tracking.";
      break;
    case 1003:
      if (set) msg = "(Xterm) Activate all motion mouse tracking.";
      else     msg = "(Xterm) Disable all motion mouse tracking.";
      break;
    case 1004:
      if (set) msg = "(Xterm) Send FocusIn/FocusOut events.";
      else     msg = "(Xterm) Don't send FocusIn/FocusOut events.";
      break;
    case 1010:
      if (set) msg = "(Rxvt) Scroll to bottom on tty output.";
      else     msg = "(Rxvt) Don't scroll to bottom on tty output.";
      break;
    case 1011:
      if (set) msg = "(Rxvt) Scroll to bottom on key press.";
      else     msg = "(Rxvt) Don't scroll to bottom on key press.";
      break;
    case 1034:
      if (set) msg = "(Xterm) Interpret meta key, sets eighth bit.";
      else     msg = "(Xterm) Don't interpret meta key.";
      break;
    case 1035:
      if (set) msg = ("(Xterm) Enable special modifiers for Alt "
                      "and NumLock keys.");
      else     msg = ("(Xterm) Disable special modifiers for Alt "
                      "and NumLock keys.");
      break;
    case 1036:
      if (set) msg = "(Xterm) Send ESC when Meta modifies a key.";
      else     msg = "(Xterm) Don't send ESC when Meta modifies a key.";
      break;
    case 1037:
      if (set) msg = "(Xterm) Send DEL from the editing-keypad Delete key.";
      else     msg = ("(Xterm) Send VT220 Remove from the "
                      "editing-keypad Delete key.");
      break;
    case 1039:
      if (set) msg = "(Xterm) Send ESC when Alt modifies a key.";
      else     msg = "(Xterm) Don't send ESC when Alt modifies a key.";
      break;
    case 1040:
      if (set) msg = "(Xterm) Keep selection even if not highlighted.";
      else     msg = ("(Xterm) Do not keep selection even if not "
                      "highlighted.");
      break;
    case 1041:
      if (set) msg = "(Xterm) Use the CLIPBOARD selection.";
      else     msg = "(Xterm) Don't use the CLIPBOARD selection.";
      break;
    case 1042:
      if (set) msg = "\
(Xterm) Enable Urgency window manager hint when BEL is received.";
      else     msg = "\
(Xterm) Disable Urgency window manager hint when BEL is received.";
      break;
    case 1043:
      if (set) msg = "\
(Xterm) Enable raising of the window when BEL is received.";
      else     msg = "\
(Xterm) Disable raising of the window when BEL is received.";
      break;
    case 1047:
      if (set) msg = "(Xterm) Use the alternate screen buffer.";
      else     msg = "(Xterm) Use the normal screen buffer.";
      break;
    case 1048:
      if (set) msg = "(Xterm) Save the cursor position.";
      else     msg = "(Xterm) Restore the cursor position.";
      break;
    case 1049:
      if (set) msg = "\
(Xterm) Save the cursor position and use the alternate screen buffer,\n\
\"  clearing it first.";
      else msg = ("(Xterm) Leave the alternate screen buffer and "
                  "restore the cursor.");
      break;
    case 2004:
      if (set) msg = "(Xterm) Set bracketed paste mode.";
      else     msg = "(Xterm) Reset bracketed paste mode.";
      break;
    }

  if (msg)
    putter_single_desc (putr, "%s", msg);
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
          putter_single_desc (putr, "%s (%s) -> %s", m->name, m->acro, arg);
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
    putter_single_desc (putr, "%s", messages[params[0]]);
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

      putter_single_desc (putr, "%s", msg);
      return;
    }
  else if (priv) return;
  if (p < N_ARY_ELEMS (messages))
    {
      putter_single_desc (putr, "%s", messages[p]);
    }
}

static void
print_sgr_param_description (struct putter *putr, unsigned int param)
{
  const char *msg = NULL;
  if (param >= 90 && param <= 107)
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
      putter_single_desc (putr, "%s", msg);
    }
  if (param == 100)
    {
      putter_single_desc (putr, "%s", ("(Rxvt) Set foreground and background "
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
      putter_single_desc (putr, "Set %s color to index %u.",
                          fore_back, params[2]);
    }
  else
    {
      putter_single_desc (putr, "Set %s color (unknown).", fore_back);
    }
}

static void
csi_do_sgr (unsigned char final, unsigned char priv, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  unsigned int *pend = params + n_params;
  unsigned int *param;
  
  assert (n_params > 0);
  if (priv == '>')
    {
      const char *res = NULL;
      int arg = 0;
      switch (params[0])
        {
        case 1: res = "modifyCursorKeys"; break;
        case 2: res = "modifyFunctionKeys"; break;
        case 4: res = "modifyOtherKeys"; break;
        }
      if (n_params > 1 && params[1] > 0)
        arg = params[1];
      if (res)
        putter_single_desc (putr, "(Xterm) Set %s to %u.", res, arg);
    }
  if (priv) return;
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
  if (priv == '>')
    {
      const char *res = NULL;
      switch (params[0])
        {
        case 1: res = "modifyCursorKeys"; break;
        case 2: res = "modifyFunctionKeys"; break;
        case 4: res = "modifyOtherKeys"; break;
        }
      if (res)
        putter_single_desc (putr, "(Xterm) Disable %s.", res);
    }
  if (priv) return;
  if (p < N_ARY_ELEMS (messages))
    putter_single_desc (putr, "%s", messages[p]);
}

static void
csi_do_sr (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  if (priv == '?')
    {
      if (final == 'r')
        putter_single_desc (putr, ("\
*** (Xterm) Restore saved settings for specified modes:"));
      else
        putter_single_desc (putr, ("\
*** (Xterm) Save current state of specified modes:"));
      csi_do_sm ('h', priv, putr, n_params, params);
    }
  else if (priv)
    return;
  else if (final != 'r')
    return;
  else if (n_params == 0)
    putter_single_desc (putr, "(DEC) Set the scrolling region to full size.");
  else if (n_params == 2)
    {
      putter_single_desc (putr, "\
(DEC) Set the scrolling region to from line %u to line %u.",
                     params[0], params[1]);
    }
}

static void
csi_do_wm (unsigned char final, unsigned char priv, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  if (priv)
    return;
  if (n_params == 0)
    return;
  switch (params[0])
    {
    case 1:
      putter_single_desc (putr, "(dtterm) De-iconify window.");
      break;
    case 2:
      putter_single_desc (putr, "(dtterm) Iconify window.");
      break;
    case 3:
      if (n_params >= 3)
        putter_single_desc (putr, "(dtterm) Move window to [%u, %u].",
                       params[1], params[2]);
      break;
    case 4:
      if (n_params >= 3)
        putter_single_desc (putr, "\
(dtterm) Resize the window to height %u and width %u in pixels.",
                       params[1], params[2]);
      break;
    case 5:
      putter_single_desc (putr, "\
(dtterm) Raise the window to the front of the stacking order.");
      break;
    case 6:
      putter_single_desc (putr, "\
(dtterm) Lower the xterm window to the bottom of the stacking order.");
      break;
    case 7:
      putter_single_desc (putr, "(dtterm) Refresh the window.");
      break;
    case 8:
      if (n_params >= 3)
        putter_single_desc (putr, "\
(dtterm) Resize the text area to height %u and width %u in characters.",
                       params[1], params[2]);
      break;
    case 9:
      if (n_params < 2)
        break;
      else if (params[1] == 0)
        putter_single_desc (putr, "(Xterm) Restore maximized window.");
      else if (params[1] == 1)
        putter_single_desc (putr, "(Xterm) Maximize window.");
      break;
    case 11:
      putter_single_desc (putr, "\
(dtterm) Request report on the window state (iconified/not iconified).");
      break;
    case 13:
      putter_single_desc (putr, "\
(dtterm) Request report on the window position.");
      break;
    case 14:
      putter_single_desc (putr, "\
(dtterm) Request report on window size in pixels.");
      break;
    case 18:
      putter_single_desc (putr, "\
(dtterm) Request report on text area size in characters.");
      break;
    case 19:
      putter_single_desc (putr, "\
(Xterm) Request report on the whole screen size in characters.");
      break;
    case 20:
      putter_single_desc (putr, "\
(dtterm) Request report of the window's icon label.");
      break;
    case 21:
      putter_single_desc (putr, "\
(dtterm) Request report of the window's title.");
      break;
    default:
      if (params[0] >= 24)
        {
          putter_single_desc (putr, "\
(Xterm) Resize the window to %u lines.", params[0]);
        }
      break;
    }
}

static void
csi_do_decelr (unsigned char final, unsigned char priv,
               struct putter *putr, size_t n_params, unsigned int *params)
{
  assert (n_params == 2);
  if (priv) return;
  if (params[0] > 2 || params[1] > 2)
    return;
  switch (params[0])
    {
    case 0:
      putter_single_desc (putr, "Disable locator reports.");
      return; /* (why mention units we won't be reporting with?) */
    case 1:
      putter_single_desc (putr, "Enable locator reports.");
      break;
    case 2:
      putter_single_desc (putr, "Enable a single locator report.");
      break;
    }
  switch (params[1])
    {
    case 0:
    case 2:
      putter_single_desc (putr, " Report position in character cells.");
      break;
    case 1:
      putter_single_desc (putr, " Report position in pixels.");
      break;
    }
}

static void
csi_do_decsle (unsigned char final, unsigned char priv,
               struct putter *putr, size_t n_params, unsigned int *params)
{
  const char *msgs[] =
    {
      "Only respond to explicit locator report requests.",
      "Report button-down transitions.",
      "Do not report button-down transitions.",
      "Report button-up transitions.",
      "Do not report button-up transitions."
    };
  unsigned int *p, *pend = params + n_params;
  
  if (priv) return;
  for (p = params; p != pend; ++p)
    if (*p < N_ARY_ELEMS (msgs))
      putter_single_desc (putr, "%s", msgs[*p]);
}

static void
csi_do_decrqlp (unsigned char final, unsigned char priv,
                struct putter *putr, size_t n_params, unsigned int *params)
{
  if (priv) return;
  if (params[0] > 1) return;
  putter_single_desc (putr, "Request a single DECLRP locator report.");
}

static void
csi_do_decmouse (unsigned char final, unsigned char priv,
                 struct putter *putr, size_t n_params, unsigned int *params)
{
  char downs[100] = " [down:", *cur;
  const char *buttons[] = {"right", "middle", "left", "M4"};
  
  if (priv) return;
  if (n_params < 4) return;
  if (params[1] == 0)
    cur = "";
  else
    {
      unsigned int bit;
      cur = downs + strlen (downs);
      for (bit = 0; bit != N_ARY_ELEMS (buttons); ++bit)
        {
          size_t len;

          if ((params[1] & (1u << bit)) == 0) continue;
          if (*(cur-1) != ':')
            *cur++ = '/';
          len = strlen (buttons[bit]);
          memcpy (cur, buttons[bit], len);
          cur += len;
        }
      *cur = ']';
      /* terminating '\0' is already there; downs was initialized. */
      assert (cur - downs < sizeof downs);
      cur = downs;
    }
  putter_single_desc (putr, "(DEC) Mouse%s at [%u,%u].",
                 cur, params[2], params[2]);
}

const static struct csi_handler csi_no_handler = { NULL, NULL };

const static struct csi_handler csi_handlers[] =
  {
    {"ICH", "INSERT CHARACTER", CSI_FUNC_PN, csi_do_ich, 1},  /* x40 */
    {"CUU", "CURSOR UP", CSI_FUNC_PN, csi_do_cuu, 1},
    {"CUD", "CURSOR DOWN", CSI_FUNC_PN, csi_do_cuu, 1},
    {"CUF", "CURSOR RIGHT", CSI_FUNC_PN, csi_do_cuu, 1},
    {"CUB", "CURSOR LEFT", CSI_FUNC_PN, csi_do_cuu, 1},
    {"CNL", "CURSOR NEXT LINE", CSI_FUNC_PN, csi_do_cnl, 1},
    {"CPL", "CURSOR PRECEDING LINE", CSI_FUNC_PN, csi_do_cnl, 1},
    {"CHA", "CURSOR CHARACTER ABSOLUTE", CSI_FUNC_PN, csi_do_cha, 1},
    {"CUP", "CURSOR POSITION", CSI_FUNC_PN_PN, csi_do_cup, 1, 1},   /* x48 */
    {"CHT", "CURSOR FORWARD TABULATION", CSI_FUNC_PN, csi_do_cht, 1},
    {"ED", "ERASE IN PAGE", CSI_FUNC_PS, csi_do_ed, 0},
    {"EL", "ERASE IN LINE", CSI_FUNC_PS, csi_do_ed, 0},
    {"IL", "INSERT LINE", CSI_FUNC_PN, csi_do_il, 1},
    {"DL", "DELETE LINE", CSI_FUNC_PN, csi_do_dl, 1},
    {"EF", "ERASE IN FIELD", CSI_FUNC_PS, csi_do_ef, 0},
    {"EA", "ERASE IN AREA"},
    {"DCH", "DELETE CHARACTER", CSI_FUNC_PN, csi_do_dch, 1},  /* x50 */
    {"SEE", "SELECT EDITING EXTENT"},
    {"CPR", "ACTIVE POSITION REPORT", CSI_FUNC_PN_PN, csi_do_cpr, 1, 1},
    {"SU", "SCROLL UP", CSI_FUNC_PN, csi_do_su, 1},
    {"SD", "SCROLL DOWN", CSI_FUNC_PN, csi_do_su, 1},
    {"NP", "NEXT PAGE"},
    {"PP", "PRECEDING PAGE"},
    {"CTC", "CURSOR TABULATION CONTROL", CSI_FUNC_PS_ANY, csi_do_ctc, 0},
    {"ECH", "ERASE CHARACTER", CSI_FUNC_PN, csi_do_ech, 1},   /* x58 */
    {"CVT", "CURSOR LINE TABULATION", CSI_FUNC_PN, csi_do_cht, 1},
    {"CBT", "CURSOR BACKWARD TABULATION", CSI_FUNC_PN, csi_do_cht, 1},
    {"SRS", "START REVERSED STRING"},
    {"PTX", "PARALLEL TEXTS"},
    {"SDS", "START DIRECTED STRING"},
    {"SIMD", "SELECT IMPLICIT MOVEMENT DIRECTION"},
    {NULL, NULL},
    {"HPA", "CHARACTER POSITION ABSOLUTE", CSI_FUNC_PN, csi_do_cha, 1},
                                                            /* ^ x60 */
    {"HPR", "CHARACTER POSITION FORWARD"},
    {"REP", "REPEAT"},
    {"DA", "DEVICE ATTRIBUTES", CSI_FUNC_PS, csi_do_da, 0},
    {"VPA", "LINE POSITION ABSOLUTE", CSI_FUNC_PN, csi_do_vpa, 1},
    {"VPR", "LINE POSITION FORWARD"},
    {"HVP", "CHARACTER AND LINE POSITION", CSI_FUNC_PN_PN, csi_do_cup, 1, 1},
    {"TBC", "TABULATION CLEAR", CSI_FUNC_PS, csi_do_tbc, 0},
    {"SM", "SET MODE", CSI_FUNC_PS_ANY, csi_do_sm},           /* x68 */
    {"MC", "MEDIA COPY", CSI_FUNC_PS, csi_do_mc, 0},
    {"HPB", "CHARACTER POSITION BACKWARD"},
    {"VPB", "LINE POSITION BACKWARD"},
    {"RM", "RESET MODE", CSI_FUNC_PS_ANY, csi_do_sm},
    {"SGR", "SELECT GRAPHIC RENDITION", CSI_FUNC_PS_ANY, csi_do_sgr, 0},
    {"DSR", "DEVICE STATUS REPORT", CSI_FUNC_PS, csi_do_dsr, 0},
    {"DAQ", "DEFINE AREA QUALIFICATION"}
 };

const static struct csi_handler csi_spc_handlers[] = 
  {
    {"SL", "SCROLL LEFT", CSI_FUNC_PN, csi_do_su, 1},
    {"SR", "SCROLL RIGHT", CSI_FUNC_PN, csi_do_su, 1},
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

const static struct csi_handler csi_sr_handler =
  {NULL, NULL, CSI_FUNC_PN_ANY, csi_do_sr, -1, -1};
const static struct csi_handler csi_wm_handler =
  {NULL, NULL, CSI_FUNC_PS_ANY, csi_do_wm, -1, -1};
const static struct csi_handler csi_decelr_handler =
  {"DECELR", "ENABLE LOCATOR REPORTING", CSI_FUNC_PS_PS, csi_do_decelr, 0, 0};
const static struct csi_handler csi_decsle_handler =
  {"DECSLE", "SELECT LOCATOR EVENTS", CSI_FUNC_PS_ANY, csi_do_decsle, 0};
const static struct csi_handler csi_decrqlp_handler =
  {"DECRQLP", "REQUEST LOCATOR POSITION", CSI_FUNC_PS, csi_do_decrqlp, 0};
const static struct csi_handler csi_decmouse_handler =
  {NULL, NULL, CSI_FUNC_PS_ANY, csi_do_decmouse, -1, -1};

const struct csi_handler *
get_csi_handler (int private_indicator, size_t intermsz,
                 int interm, unsigned char final)
{
  if (final >= 0x70)
    {
      if (intermsz == 0)
        {
          switch (final)
            {
            case 'r':
            case 's':
              return &csi_sr_handler;
            case 't':
              return &csi_wm_handler;
            default:
              return &csi_no_handler;
            }
        }
      else if (intermsz == 1 && interm == '\'')
        {
          switch (final)
            {
            case 'z':
              return &csi_decelr_handler;
            case '{':
              return &csi_decsle_handler;
            case '|':
              return &csi_decrqlp_handler;
            default:
              return &csi_no_handler;
            }
        }
      else if (intermsz == 1 && interm == '&' && final == 'w')
        return &csi_decmouse_handler;
      else
        return &csi_no_handler;
    }
  else if (intermsz == 0)
    return &csi_handlers[final - 0x40];
  else if (intermsz == 1 && interm == 0x20)
    return &csi_spc_handlers[final - 0x40];
  else
    return &csi_no_handler;
}
