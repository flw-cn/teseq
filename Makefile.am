AUTOMAKE_OPTIONS = subdir-objects
AM_CFLAGS=-Wall -g -O2 -ansi -pedantic-errors
CHECKMK = checkmk

bin_PROGRAMS = eseq
bin_SCRIPTS  = reseq
libexec_SCRIPTS = src/post.sed

eseq_SOURCES = src/eseq.c src/inputbuf.c src/ringbuf.c src/putter.c \
               src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h

check_PROGRAMS = src/test-ringbuf src/test-inputbuf
check_SCRIPTS = tests/run
src_test_ringbuf_SOURCES = src/test-ringbuf.o

SUFFIXES = .cm

.cm.c:
	$(CHECKMK) $< > $@
