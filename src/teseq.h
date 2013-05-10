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

#ifndef TESEQ_H_
#define TESEQ_H_

#include "config.h"

#define _XOPEN_SOURCE 600

#include <stdio.h>

#define N_ARY_ELEMS(ary)        (sizeof (ary) / sizeof (ary)[0])

enum {
    CFG_COLOR_NONE,
    CFG_COLOR_AUTO,
    CFG_COLOR_ALWAYS,
    CFG_COLOR_SET       /* Used temporarily when processing options. */
};

struct config
{
  int control_hats;
  int descriptions;
  int labels;
  int escapes;
  int buffered;
  int handle_signals;
  FILE *timings;
  int color;
};
extern struct config configuration;

struct sgr_def {
    const char *sgr;
    unsigned int len;
};

extern struct sgr_def        sgr_text, sgr_text_decor, sgr_ctrl, sgr_esc,
                             sgr_label, sgr_desc, sgr_delay;

#endif /* TESEQ_H_ */
