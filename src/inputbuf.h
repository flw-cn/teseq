/* inputbuf.h */

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

/*
    Input buffer.

    Fetches characters one at a time from an input stream.  The
    inputbuf_saving function is used to initiate "look-ahead" mode, to
    begin saving the characters into a buffer. The inputbuf_rewind
    function is used to re-read the saved characters; and
    inputbuf_forget is used to indicate that we are done processing
    the saved characters, and they should be forgotten.

    See test-inputbuf.cm for usage.
*/

#ifndef INPUTBUF_H
#define INPUTBUF_H

#include <stdio.h>

struct inputbuf;

struct inputbuf *inputbuf_new (FILE *, size_t);
void inputbuf_delete (struct inputbuf *);
int inputbuf_io_error (struct inputbuf *);

int inputbuf_get (struct inputbuf *);
int inputbuf_saving (struct inputbuf *);
int inputbuf_rewind (struct inputbuf *);
int inputbuf_forget (struct inputbuf *);
size_t inputbuf_get_count (struct inputbuf *);
void inputbuf_reset_count (struct inputbuf *);
int inputbuf_avail (struct inputbuf *);

#endif
