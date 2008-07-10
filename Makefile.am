AUTOMAKE_OPTIONS = subdir-objects
AM_CFLAGS=-Wall -g -O2 -ansi -pedantic-errors
CHECKMK = checkmk

bin_PROGRAMS = eseq
bin_SCRIPTS  = reseq
libexec_SCRIPTS = src/post.sed

eseq_SOURCES = src/eseq.c src/inputbuf.c src/ringbuf.c src/putter.c \
               src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h

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

check-eseq:
	cd $(top_srcdir)/tests && ./run

check-reseq:
	cd $(top_srcdir)/tests && ./run -r
