PROG	= lr

CC		= gcc
CFLAGS	+= -Os -s
STRIP	= strip --strip-unneeded
VPATH	= ./src
LIBS	= -llua

all: $(PROG)

$(PROG): main.c
		$(CC) -o $@ $< $(LIBS) $(CFLAGS) 
		$(STRIP) $@

clean:
	rm $(PROG)

.PHONY: clean

