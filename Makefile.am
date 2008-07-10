AM_CFLAGS=-Wall -g -O2 -ansi -pedantic-errors
CHECKMK = checkmk

bin_PROGRAMS = eseq reseq

eseq_SOURCES = src/eseq.c src/inputbuf.c src/ringbuf.c src/putter.c \
               src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h
reseq_SOURCES =

check_PROGRAMS = src/test-ringbuf src/test-inputbuf tests/run

SUFFIXES = .cm

.cm.c:
	$(CHECKMK) $< > $@
