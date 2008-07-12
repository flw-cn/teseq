AUTOMAKE_OPTIONS = subdir-objects
AM_CFLAGS=-Wall -g -O2 -ansi -pedantic-errors
CHECKMK = checkmk

bin_PROGRAMS = eseq
dist_bin_SCRIPTS  = reseq
dist_libexec_SCRIPTS = src/eseq-post.sed

eseq_SOURCES = src/eseq.c src/inputbuf.c src/ringbuf.c src/putter.c \
               src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h \
               src/c1.h src/eseq.h

check_PROGRAMS = src/test-ringbuf src/test-inputbuf
check_DATA = src/test-inputbuf.inf
TESTS = $(check_PROGRAMS)
src_test_ringbuf_SOURCES = src/test-ringbuf.c src/ringbuf.c
src_test_ringbuf_LDADD = -lcheck
src_test_inputbuf_SOURCES = src/test-inputbuf.c src/inputbuf.c src/ringbuf.c
src_test_inputbuf_LDADD = -lcheck

SUFFIXES = .cm

.cm.c:
	$(CHECKMK) $< > $@

check: check-eseq check-reseq

check-eseq: eseq
	./run-tests

check-reseq: reseq
	./run-tests -r

EXTRA_DIST=src/test-inputbuf.inf

dist-hook:
	cp -R -- `find $(srcdir)/tests -type d ! -name tests` $(distdir)/tests

clean-local:
	find tests -name r-output -o -name output | xargs rm -f
	rm -f tests/cmdline--/-o
	test "$(srcdir)" = . || rm -f src/test-inputbuf.inf
