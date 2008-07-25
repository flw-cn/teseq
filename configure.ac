#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([teseq], [1.0-devel], [micah@cowan.name])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2 gnu])
AC_CONFIG_SRCDIR([src/teseq.c])
AC_CONFIG_HEADER([src/config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([check], tcase_add_test)

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
AC_CONFIG_FILES([run-tests:tests/run.in],[chmod +x run-tests])
AC_CONFIG_COMMANDS([src/test-inputbuf.inf],
                   [test "$srcdir" = . || \
                    cp "$srcdir/src/test-inputbuf.inf" \
                       src/test-inputbuf.inf])
AC_CONFIG_COMMANDS([doc],
                   [mkdir -p doc])
AC_OUTPUT
