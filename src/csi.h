/* csi.h */

#include <stddef.h>

const char *csi_labels[][2] = {
	{"ICH", "INSERT CHARACTER"}, /* x40 */
	{"CUU", "CURSOR UP"},
	{"CUD", "CURSOR DOWN"},
	{"CUF", "CURSOR RIGHT"},
	{"CUB", "CURSOR LEFT"},
	{"CNL", "CURSOR NEXT LINE"},
	{"CPL", "CURSOR PRECEDING LINE"},
	{"CHA", "CURSOR CHARACTER ABSOLUTE"},
	{"CUP", "CURSOR POSITION"}, /* x48 */
	{"CHT", "CURSOR FORWARD TABULATION"},
	{"ED", "ERASE IN PAGE"},
	{"EL", "ERASE IN LINE"},
	{"IL", "INSERT LINE"},
	{"DL", "DELETE LINE"},
	{"EF", "ERASE IN FIELD"},
	{"EA", "ERASE IN AREA"},
	{"DCH", "DELETE CHARACTER"}, /* x50 */
	{"SEE", "SELECT EDITING EXTENT"},
	{"CPR", "ACTIVE POSITION REPORT"},
	{"SU", "SCROLL UP"},
	{"SD", "SCROLL DOWN"},
	{"NP", "NEXT PAGE"},
	{"PP", "PRECEDING PAGE"},
	{"CTC", "CURSOR TABULATION CONTROL"},
	{"ECH", "ERASE CHARACTER"}, /* x58 */
	{"CVT", "CURSOR LINE TABULATION"},
	{"CBT", "CURSOR BACKWARD TABULATION"},
	{"SRS", "START REVERSED STRING"},
	{"PTX", "PARALLEL TEXTS"},
	{"SDS", "START DIRECTED STRING"},
	{"SIMD", "SELECT IMPLICIT MOVEMENT DIRECTION"},
	{NULL, NULL},
	{"HPA", "CHARACTER POSITION ABSOLUTE"}, /* x60 */
	{"HPR", "CHARACTER POSITION FORWARD"},
	{"REP", "REPEAT"},
	{"DA", "DEVICE ATTRIBUTES"},
	{"VPA", "LINE POSITION ABSOLUTE"},
	{"VPR", "LINE POSITION FORWARD"},
	{"HVP", "CHARACTER AND LINE POSITION"},
	{"TBC", "TABULATION CLEAR"},
	{"SM", "SET MODE"}, /* x68 */
	{"MC", "MEDIA COPY"},
	{"HPB", "CHARACTER POSITION BACKWARD"},
	{"VPB", "LINE POSITION BACKWARD"},
	{"RM", "RESET MODE"},
	{"SGR", "SELECT GRAPHIC RENDITION"},
	{"DSR", "DEVICE STATUS REPORT"},
	{"DAQ", "DEFINE AREA QUALIFICATION"}
};

/* vim:set sw=8 ts=8 sts=8 noet: */