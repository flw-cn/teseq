/* modes.h */

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
