# Process this file with automake to produce Makefile.in (in this,
# and all subdirectories).
#
# Makefile for the top-level directory of GNU teseq.
# Copyright (C) 2008 Micah Cowan.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AUTOMAKE_OPTIONS = subdir-objects
# AM_CFLAGS=-Wall -g -O2 -ansi -pedantic-errors

bin_PROGRAMS = teseq
dist_bin_SCRIPTS  = reseq

teseq_SOURCES = src/teseq.c src/inputbuf.c src/ringbuf.c src/putter.c \
                src/csi.c \
                src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h \
                src/c1.h src/teseq.h src/modes.h

if DO_CHECK_TESTS
  check_PROGRAMS = src/test-ringbuf src/test-inputbuf
  check_DATA = src/test-inputbuf.inf
  TESTS = $(check_PROGRAMS)
endif
src_test_ringbuf_SOURCES = src/test-ringbuf.c src/ringbuf.c
src_test_ringbuf_LDADD = @LIBCHECK@
src_test_inputbuf_SOURCES = src/test-inputbuf.c src/inputbuf.c src/ringbuf.c
src_test_inputbuf_LDADD = @LIBCHECK@

info_TEXINFOS = doc/teseq.texi
doc_teseq_TEXINFOS = doc/fdl.texi
dist_man_MANS = doc/teseq.1 doc/reseq.1

SUFFIXES = .cm

doc/teseq.1: src/teseq.c
	$(HELP2MAN) ./teseq -o doc/teseq.1 -n "Format text with terminal escapes and control sequences for human consumption."

doc/reseq.1: ./reseq.in
	$(HELP2MAN) ./reseq -o doc/reseq.1 -n "Reverse the translations made by teseq."

.cm.c:
	$(CHECKMK) $< > $@ || ( rm -f $@ && false )

check: check-teseq check-reseq

check-teseq: teseq run-tests
	./run-tests

check-reseq: reseq run-tests
	./run-tests -r

EXTRA_DIST=src/test-inputbuf.inf

dist-hook:
	cp -R -- `find $(srcdir)/tests -type d ! -name tests` $(distdir)/tests

clean-local:
	find tests -name r-output -o -name output | xargs rm -f
	rm -f tests/cmdline--/-o
	rm -f tests/timing/timing-out
	test "$(srcdir)" = . || rm -f src/test-inputbuf.inf
	rm -f doc/teseq.1 doc/reseq.1

CLEANFILES = teseq.dvi
