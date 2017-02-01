include config.mk

SRC = svi.c util.c
OBJ = ${SRC:.c=.o}

all: svi

svi: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

.c.o:
	${CC} -c ${CFLAGS} $<

clean:
	rm -f svi ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f svi ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/svi

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/svi
