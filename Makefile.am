AUTOMAKE_OPTIONS = subdir-objects
AM_CFLAGS=-Wall -g -O2 -ansi -pedantic-errors
CHECKMK = checkmk

bin_PROGRAMS = teseq
dist_bin_SCRIPTS  = reseq
dist_libexec_SCRIPTS = src/teseq-post.sed

teseq_SOURCES = src/teseq.c src/inputbuf.c src/ringbuf.c src/putter.c \
                src/csi.c \
                src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h \
                src/c1.h src/teseq.h src/modes.h

check_PROGRAMS = src/test-ringbuf src/test-inputbuf
check_DATA = src/test-inputbuf.inf
TESTS = $(check_PROGRAMS)
src_test_ringbuf_SOURCES = src/test-ringbuf.c src/ringbuf.c
src_test_ringbuf_LDADD = -lcheck
src_test_inputbuf_SOURCES = src/test-inputbuf.c src/inputbuf.c src/ringbuf.c
src_test_inputbuf_LDADD = -lcheck

info_TEXINFOS = doc/teseq.texi
doc_teseq_TEXINFOS = doc/fdl.texi
dist_man_MANS = doc/teseq.1 doc/reseq.1

SUFFIXES = .cm

doc/teseq.1: ./teseq
	help2man ./teseq -o doc/teseq.1

doc/reseq.1: ./reseq
	help2man ./reseq -o doc/reseq.1

.cm.c:
	$(CHECKMK) $< > $@

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
	test "$(srcdir)" = . || rm -f src/test-inputbuf.inf doc/teseq.1 \
	                              doc/reseq.1

CLEANFILES = teseq.dvi
