/* modes.h */

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

struct mode_info
{
  const char *acro;
  const char *name;
  const char *reset;
  const char *set;
  const char *reset_desc;
  const char *set_desc;
};

struct mode_info modes[] =
  {
    {NULL},
    {"GATM",    "GUARDED AREA TRANSFER MODE",   "GUARD", "ALL"},
    {"KAM",     "KEYBOARD ACTION MODE",         "ENABLED", "DISABLED"},
    {"CRM",     "CONTROL REPRESENTATION MODE",  "CONTROL", "GRAPHIC"},
    {"IRM",     "INSERTION REPLACEMENT MODE",   "REPLACE", "INSERT"},
    {"SRTM",    "STATUS REPORT TRANSFER MODE",  "NORMAL", "DIAGNOSTIC"},
    {"ERM",     "ERASURE MODE",                 "PROTECT", "ALL"},
    {"VEM",     "LINE EDITING MODE",            "FOLLOWING", "PRECEDING"},
    {"BDSM",    "BI-DIRECTIONAL SUPPORT MODE",  "EXPLICIT", "IMPLICIT"},
    {"DCSM",    "DEVICE COMPONENT SELECT MODE", "PRESENTATION", "DATA"},
    {"HEM",     "CHARACTER EDITING MODE",       "FOLLOWING", "PRECEDING"},
    {"PUM",     "POSITIONING UNIT MODE",        "CHARACTER", "SIZE"},
    {"SRM",     "SEND/RECEIVE MODE",            "MONITOR", "SIMULTANEOUS"},
    {"FEAM",    "FORMAT EFFECTOR ACTION MODE",  "EXECUTE", "STORE"},
    {"FETM",    "FORMAT EFFECTOR TRANSFER MODE","INSERT", "EXCLUDE"},
    {"MATM",    "MULTIPLE AREA TRANSFER MODE",  "SINGLE", "MULTIPLE"},
    {"TTM",     "TRANSFER TERMINATION MODE",    "CURSOR", "ALL"},
    {"SATM",    "SELECTED AREA TRANSFER MODE",  "SELECT", "ALL"},
    {"TSM",     "TABULATION STOP MODE",         "MULTIPLE", "SINGLE"},
    {NULL},
    {NULL},
    {"GRCM",    "GRAPHIC RENDITION COMBINATION GRCM", "REPLACING", "CUMULATIVE"},
    {"ZDM",     "ZERO DEFAULT MODE",            "ZERO", "DEFAULT"}
  };
