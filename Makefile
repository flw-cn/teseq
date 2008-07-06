CFLAGS=-Wall -g
CHECKMK = checkmk
ESEQ_SOURCES = src/eseq.c src/inputbuf.c src/ringbuf.c src/putter.c
ESEQ_INCLUDES = src/sgr.h src/csi.h src/inputbuf.h src/ringbuf.h src/putter.h
PREFIX = /usr/local
EXEC_PREFIX = $(PREFIX)/bin
LIBEXEC_PREFIX = $(PREFIX)/libexec/eseq
INSTALL = install

all: eseq

eseq: $(ESEQ_SOURCES) $(ESEQ_INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESEQ_SOURCES) -o $@

check: unit-tests functionality-tests

.PHONY: unit-tests
unit-tests: check-ringbuf check-inputbuf

.PHONY: check-ringbuf
check-ringbuf: src/test-ringbuf
	src/test-ringbuf

.PHONY: check-inputbuf
check-inputbuf: src/test-inputbuf
	cd src && ./test-inputbuf

src/test-ringbuf: src/ringbuf.h src/ringbuf.c src/test-ringbuf.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/ringbuf.c src/test-ringbuf.c -lcheck -o $@

src/test-inputbuf: src/inputbuf.h src/inputbuf.c src/test-inputbuf.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/inputbuf.c src/test-inputbuf.c \
	    src/ringbuf.c -lcheck -o $@

.SUFFIXES: .cm

.cm.c:
	#cd src && $(CHECKMK) $$(basename $<) > $$(basename $@)
	$(CHECKMK) $< > $@

.PHONY: functionality-tests
functionality-tests: eseq
	cd tests && ./run

.PHONY: clean
clean:
	rm -f eseq src/test-ringbuf tests/*/output

.PHONY: install
install: $(EXEC_PREFIX)/eseq $(LIBEXEC_PREFIX)/post.sed

$(EXEC_PREFIX):
	mkdir -p $@

$(LIBEXEC_PREFIX):
	mkdir -p $@

$(EXEC_PREFIX)/eseq: $(EXEC_PREFIX) eseq
	$(INSTALL) eseq $@

$(LIBEXEC_PREFIX)/post.sed: $(LIBEXEC_PREFIX) src/post.sed
	$(INSTALL) src/post.sed $@
