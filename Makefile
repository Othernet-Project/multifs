#CC?=gcc

CFLAGS+=-Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
LDLIBS+=-lfuse -pthread

multifs: main.c tools.c usage.c parse_options.c flist.c debug.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) -f multifs

