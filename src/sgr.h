/* sgr.h */

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


#include <stddef.h>

const char *sgr_param_descriptions[] = {
  "Clear graphic rendition to defaults.",
  "Set bold text.",
  "Set dim text.",
  "Set italicized text.",
  "Set underlined text.",
  "Set slowly blinking text.",  /* 5 */
  "Set rapidly blinking text.",
  "Set negative text image.",
  "Set hidden text.",
  "Set strike-out text.",
  "Set default font.",          /* 10 */
  "Set first alternative font.",
  "Set second alternative font.",
  "Set third alternative font.",
  "Set fourth alternative font.",
  "Set fifth alternative font.",        /* 15 */
  "Set sixth alternative font.",
  "Set seventh alternative font.",
  "Set eighth alternative font.",
  "Set ninth alternative font.",
  "Set Fraktur (Gothic) font.", /* 20 */
  "Set double-underlined text.",
  "Clear bold or dim text.",
  "Clear italicized or fraktur text.",
  "Clear underlining.",
  "Clear blinking.",            /* 25 */
  NULL,                         /* Reserved for T.61, which was not published. */
  "Set positive text image.",
  "Set visible text.",
  "Clear strike-out text.",
  "Set foreground color black.",        /* 30 */
  "Set foreground color red.",
  "Set foreground color green.",
  "Set foreground color yellow.",
  "Set foreground color blue.",
  "Set foreground color magenta.",      /* 35 */
  "Set foreground color cyan.",
  "Set foreground color white.",
  NULL,                         /* XXX: T.416 */
  "Set foreground color default.",
  "Set background color black.",        /* 40 */
  "Set background color red.",
  "Set background color green.",
  "Set background color yellow.",
  "Set background color blue.",
  "Set background color magenta.",      /* 45 */
  "Set background color cyan.",
  "Set background color white.",
  NULL,                         /* XXX: T.416 */
  "Set background color default.",
  NULL,                         /* Reserved for T.61, which was not published. *//* 50 */
  "Set framed text.",
  "Set encircled text.",
  "Set overlined text.",
  "Clear framed or encircled text.",
  "Clear overlined text.",      /* 55 */
  NULL,                         /* Reserved for future */
  NULL,                         /* Reserved for future */
  NULL,                         /* Reserved for future */
  NULL,                         /* Reserved for future */
  "Set ideogram underline",     /* 60 */
  "Set ideogram double underline",
  "Set ideogram overline",
  "Set ideogram double overline",
  "Set ideogram stress marking",
  "Clear ideographic underlines, overlines, or stress marks."
};
