/* c1.h */

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


const char *c1_labels[][2] = {
  {NULL, NULL},
  {NULL, NULL},
  {"BPH", "BREAK PERMITTED HERE"},
  {"NBH", "NO BREAK HERE"},
  {NULL, NULL},
  {"NEL", "NEXT LINE"},
  {"SSA", "START OF SELECTED AREA"},
  {"ESA", "END OF SELECTED AREA"},
  {"HTS", "CHARACTER TABULATION SET"},
  {"HTJ", "CHARACTER TABULATION WITH JUSTIFICATION"},
  {"VTS", "LINE TABULATION SET"},
  {"PLD", "PARTIAL LINE FORWARD"},
  {"PLU", "PARTIAL LINE BACKWARD"},
  {"RI", "REVERSE LINE FEED"},
  {"SS2", "SINGLE-SHIFT TWO"},
  {"SS3", "SINGLE-SHIFT THREE"},
  {"DCS", "DEVICE CONTROL STRING"},
  {"PU1", "PRIVATE USE ONE"},
  {"PU2", "PRIVATE USE TWO"},
  {"STS", "SET TRANSMIT STATE"},
  {"CCH", "CANCEL CHARACTER"},
  {"MW", "MESSAGE WAITING"},
  {"SPA", "START OF GUARDED AREA"},
  {"EPA", "END OF GUARDED AREA"},
  {"SOS", "START OF STRING"},
  {NULL, NULL},
  {"SCI", "SINGLE CHARACTER INTRODUCER"},
  {"CSI", "CONTROL SEQUENCE INTRODUCER"},
  {"ST", "STRING TERMINATOR"},
  {"OSC", "OPERATING SYSTEM COMMAND"},
  {"PM", "PRIVACY MESSAGE"},
  {"APC", "APPLICATION PROGRAM COMMAND"}
};
