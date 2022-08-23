LIBS=	-lelf
NASM?=	nasm

all:	bsd-ce loader nk-dump

bsd-ce:	bsd-ce.c
	$(CC) -o bsd-ce bsd-ce.c $(LIBS)

loader:	loader.asm
	$(NASM) -fbin -oloader loader.asm

nk-dump: nk-dump.c
	$(CC) -o nk-dump nk-dump.c

clean:
	rm -f bsd-ce loader nk-dump
