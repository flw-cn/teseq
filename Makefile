CFLAGS=-Wall -g
CHECKMK = checkmk
ESEQ_SOURCES = src/eseq.c

all: eseq

eseq: $(ESEQ_SOURCES)
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
