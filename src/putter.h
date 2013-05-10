/* putter.h */

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
    The putter handles automatic line-length tracking, and splits the
    line where appropriate. Via the arguments to the putter_start
    function, the user tells putter what should go at the beginning
    and ends of continuation lines.
*/

#ifndef PUTTER_H
#define PUTTER_H

#include "teseq.h"

#include <stdio.h>

struct putter;

/* A putter_error_handler gets the current value of errno, and the
   arg passed to putter_set_handler. */
typedef void (*putter_error_handler)(int, void *);

struct putter *putter_new (FILE *);
void putter_set_handler (struct putter *, putter_error_handler, void *);
void putter_delete (struct putter *);
void putter_start (struct putter *, struct sgr_def *, struct sgr_def *,
                   const char *, const char *, const char *);
void putter_finish (struct putter *, const char *);
void putter_putc (struct putter *, unsigned char);
void putter_puts (struct putter *, const char *);
void putter_printf (struct putter *, const char *, ...);

void putter_single_esc   (struct putter *, const char *, ...);
void putter_single_delay (struct putter *, const char *, ...);
void putter_single_label (struct putter *, const char *, ...);
void putter_single_desc  (struct putter *, const char *, ...);

#endif
