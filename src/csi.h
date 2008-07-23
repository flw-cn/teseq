/* csi.h */

#ifndef ESEQ_CSI_H
#define ESEQ_CSI_H

#include "eseq.h"

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

typedef void (*csi_handler_func) (struct putter *, size_t, unsigned int []);

struct csi_handler
{
  const char            *acro;
  const char            *label;
  enum csi_func_type    type;
  csi_handler_func      fn;
  int                   default0;
  int                   default1;
};

extern struct csi_handler csi_handlers[];
extern struct csi_handler csi_spc_handlers[];
extern struct csi_handler csi_no_handler;

#endif /* ESEQ_CSI_H */
