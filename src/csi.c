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

#include "eseq.h"

#include <assert.h>

#include "csi.h"
#include "sgr.h"

static void
csi_do_ich (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  putter_single (putr, ("\" Shift characters after the cursor to make room "
                        "for %d new character%s."), params[0],
                 params[0] == 1 ? "" : "s");
}

static void
csi_do_cuu (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *dir[] = {"up", "down", "right", "left"};
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor %s %d line%s.",
                 dir[ final - 0x41 ],
                 params[0], params[0] == 1 ? "" : "s");
}

static void
csi_do_cnl (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  const char *dir[] = {"down", "up"};
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor to the first column, %d line%s %s.",
                 params[0], params[0] == 1 ? "" : "s", dir[ final - 0x45 ]);
}

static void
csi_do_cha (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor to column %d.", params[0]);
}

static void
csi_do_cup (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  assert (n_params == 2);
  putter_single (putr, "\" Move the cursor to line %d, column %d.",
                 params[0], params[1]);
}

static void
csi_do_cht (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  putter_single (putr, "\" Move the cursor forward %d tab stops.",
                 params[0]);
}

static void
csi_do_ed (unsigned char final, struct putter *putr,
           size_t n_params, unsigned int *params)
{
  assert (n_params == 1);
  switch (params[0])
    {
    case 0:
      putter_single (putr, "\" Clear the screen.");
      break;
    case 1:
      putter_single (putr, ("\" Clear from the beginning of the screen "
                            " to the cursor."));
      break;
    case 2:
      putter_single (putr, ("\" Clear from the cursor to the end of "
                            "the screen."));
      break;
    }
}

static void
print_sgr_param_description (struct putter *putr, unsigned int param)
{
  const char *msg = NULL;
  if (param < N_ARY_ELEMS (sgr_param_descriptions))
    msg = sgr_param_descriptions[param];
  if (msg)
    {
      putter_single (putr, "\" %s", msg);
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
csi_do_sgr (unsigned char final, struct putter *putr,
            size_t n_params, unsigned int *params)
{
  unsigned int *pend = params + n_params;
  unsigned int *param;
  
  assert (n_params > 0);
  if (n_params >= 2 && (params[0] == 48 || params[0] == 38))
    print_t416_description (putr, n_params, params);
  else
    for (param = params; param != pend; ++param)
      {
        print_sgr_param_description (putr, *param);
      }
}

struct csi_handler csi_no_handler = { NULL, NULL };

struct csi_handler csi_handlers[] =
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
    {"EL", "ERASE IN LINE"},
    {"IL", "INSERT LINE"},
    {"DL", "DELETE LINE"},
    {"EF", "ERASE IN FIELD"},
    {"EA", "ERASE IN AREA"},
    {"DCH", "DELETE CHARACTER"},  /* x50 */
    {"SEE", "SELECT EDITING EXTENT"},
    {"CPR", "ACTIVE POSITION REPORT"},
    {"SU", "SCROLL UP"},
    {"SD", "SCROLL DOWN"},
    {"NP", "NEXT PAGE"},
    {"PP", "PRECEDING PAGE"},
    {"CTC", "CURSOR TABULATION CONTROL"},
    {"ECH", "ERASE CHARACTER"},   /* x58 */
    {"CVT", "CURSOR LINE TABULATION"},
    {"CBT", "CURSOR BACKWARD TABULATION"},
    {"SRS", "START REVERSED STRING"},
    {"PTX", "PARALLEL TEXTS"},
    {"SDS", "START DIRECTED STRING"},
    {"SIMD", "SELECT IMPLICIT MOVEMENT DIRECTION"},
    {NULL, NULL},
    {"HPA", "CHARACTER POSITION ABSOLUTE"},       /* x60 */
    {"HPR", "CHARACTER POSITION FORWARD"},
    {"REP", "REPEAT"},
    {"DA", "DEVICE ATTRIBUTES"},
    {"VPA", "LINE POSITION ABSOLUTE"},
    {"VPR", "LINE POSITION FORWARD"},
    {"HVP", "CHARACTER AND LINE POSITION"},
    {"TBC", "TABULATION CLEAR"},
    {"SM", "SET MODE"},           /* x68 */
    {"MC", "MEDIA COPY"},
    {"HPB", "CHARACTER POSITION BACKWARD"},
    {"VPB", "LINE POSITION BACKWARD"},
    {"RM", "RESET MODE"},
    {"SGR", "SELECT GRAPHIC RENDITION", CSI_FUNC_PS_ANY, csi_do_sgr, 0 },
    {"DSR", "DEVICE STATUS REPORT"},
    {"DAQ", "DEFINE AREA QUALIFICATION"}
  };

struct csi_handler csi_spc_handlers[] = 
  {
    {"SL", "SCROLL LEFT"},
    {"SR", "SCROLL RIGHT"},
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

