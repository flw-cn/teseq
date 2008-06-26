ESEQ_SOURCES = src/eseq.c

all: build/eseq

build/eseq: build $(ESEQ_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESEQ_SOURCES) -o $@

build:
	mkdir -p $@

check: build/eseq
	cd tests && ./run
