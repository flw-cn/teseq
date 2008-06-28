CFLAGS=-Wall -g
CHECKMK = checkmk
ESEQ_SOURCES = src/eseq.c

all: eseq

eseq: $(ESEQ_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESEQ_SOURCES) -o $@

check: unit-tests functionality-tests

.PHONY: unit-tests
unit-tests: src/test-ringbuf
	src/test-ringbuf

src/test-ringbuf: src/ringbuf.h src/ringbuf.c src/test-ringbuf.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/ringbuf.c src/test-ringbuf.c -lcheck -o $@

.SUFFIXES: .cm

.cm.c:
	cd src && $(CHECKMK) $$(basename $<) > $$(basename $@)

.PHONY: functionality-tests
functionality-tests: eseq
	cd tests && ./run

.PHONY: clean
clean:
	rm -f eseq src/test-ringbuf tests/*/output
