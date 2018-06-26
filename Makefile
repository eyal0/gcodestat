CC     := gcc
CFLAGS := -Wall -Werror
SLIBS  := -static -static-libgcc -lgcc
LIBS   := -lm


SRCS   := gcodestat.c calcmove.c readconfig.c readgcode.c
HFILES := gcodestat.h calcmove.h readconfig.h readgcode.h

OBJS   := ${SRCS:.c=.o} 
PROGS  := gcodestat.exe

.PHONY: all
all: ${PROGS}

${PROGS}: ${OBJS} Makefile
	${CC}  ${OBJS} ${ARCH} ${LFLAGS} ${SLIBS} ${LIBS} -g -o $@ 

clean:
	rm -f ${PROGS} ${OBJS}
	${CC} --version

%.o: %.c Makefile
	${CC} ${CFLAGS} -g -c $<


