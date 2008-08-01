dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.
dnl
dnl This file is free software; as a special exception the author gives
dnl unlimited permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
dnl implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


AC_PREREQ(2.61)
AC_INIT([GNU teseq], [1.0.0-devel], [bug-teseq@gnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2 dist-lzma gnu])
AC_CONFIG_SRCDIR([src/teseq.c])
AC_CONFIG_HEADER([src/config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AM_MISSING_PROG([HELP2MAN], [help2man])
AM_MISSING_PROG([CHECKMK], [checkmk])

# Checks for libraries.

AC_SUBST([LIBCHECK])
AC_CHECK_LIB([check], [suite_add_tcase],
  [AC_DEFINE([HAVE_LIBCHECK], 1,
             [Define to 1 if you have the `check' library (-lcheck).])
    LIBCHECK="-lcheck"
  ], [AC_MSG_WARN([

You do not have Check (http://check.sourceforge.net/);
You will not be able to run the full set of tests.
])
])
AM_CONDITIONAL([DO_CHECK_TESTS], [test -n "$LIBCHECK"])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_SETVBUF_REVERSED
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strchr])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([reseq],[chmod +x reseq])
AC_CONFIG_FILES([run-tests:tests/run.in],[chmod +x run-tests])
AC_CONFIG_COMMANDS([src/test-inputbuf.inf],
                   [test "$srcdir" = . || \
                    cp "$srcdir/src/test-inputbuf.inf" \
                       src/test-inputbuf.inf])
AC_CONFIG_COMMANDS([doc],
                   [mkdir -p doc])
AC_OUTPUT
