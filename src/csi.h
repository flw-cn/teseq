/* csi.h */

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


#ifndef TESEQ_CSI_H
#define TESEQ_CSI_H

#include "teseq.h"

#include <stddef.h>

#include "putter.h"

enum csi_func_type
  {
    CSI_FUNC_NONE,
    CSI_FUNC_PN,
    CSI_FUNC_PN_PN,
    CSI_FUNC_PN_ANY,
    CSI_FUNC_PS,
    CSI_FUNC_PS_PS,
    CSI_FUNC_PS_ANY
  };

#define CSI_USE_DEFAULT1(type)  ((type) == CSI_FUNC_PN_PN \
                                 || (type) == CSI_FUNC_PS_PS)

#define CSI_GET_DEFAULT(h, n)   (((n) == 1 && CSI_USE_DEFAULT1 ((h)->type)) \
                                 ? h->default1 : h->default0)

#define CSI_DEFAULT_NONE        -1

typedef void (*csi_handler_func) (unsigned char, unsigned char,
                                  struct putter *,
                                  size_t, unsigned int []);

struct csi_handler
{
  const char            *acro;
  const char            *label;
  enum csi_func_type    type;
  csi_handler_func      fn;
  int                   default0;
  int                   default1;
};

const struct csi_handler *
get_csi_handler (int private_indicator, size_t intermsz,
                 int interm, unsigned char final);

#endif /* TESEQ_CSI_H */
