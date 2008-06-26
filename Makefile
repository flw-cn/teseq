ESEQ_SOURCES = src/eseq.c

all: eseq

eseq: $(ESEQ_SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(ESEQ_SOURCES) -o $@

check: eseq
	cd tests && ./run
