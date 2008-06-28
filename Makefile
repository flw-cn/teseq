CHECKMK = checkmk
ESEQ_SOURCES = src/eseq.c

all: eseq

eseq: $(ESEQ_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESEQ_SOURCES) -o $@

check: unit-tests functionality-tests

.PHONY: unit-tests
unit-tests: src/test-buffer
	src/test-buffer

src/test-buffer: src/ringbuf.h src/ringbuf.c src/test-ringbuf.c
	$(CC) $(CPPFLAGS) $(CFLAGS) src/ringbuf.c src/test-ringbuf.c -lcheck -o $@

.SUFFIXES: .cm

.cm.c:
	$(CHECKMK) $< > $@

.PHONY: functionality-tests
functionality-tests: eseq
	cd tests && ./run
