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

#ifndef TESEQ_H_
#define TESEQ_H_

#include "config.h"

#define _XOPEN_SOURCE 600

#include <stdio.h>

#define N_ARY_ELEMS(ary)        (sizeof (ary) / sizeof (ary)[0])

struct config
{
  int control_hats;
  int descriptions;
  int labels;
  int escapes;
  int extensions;
  FILE *timings;
};

extern struct config configuration;

#endif /* TESEQ_H_ */
