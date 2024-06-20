HEADER_FILES = filemap.h uthash.h utlist.h
SOURCE_FILES = dirents.c extents.c main.c options.c print.c sort.c
OBJECT_FILES = ${SOURCE_FILES:.c=.o}
EXECUTABLE = filemap

CC ?= gcc
CFLAGS ?= -O2
LDFLAGS ?= -O2

CFLAGS += -std=c99 -D_DEFAULT_SOURCE=1 -D_POSIX_C_SOURCE=200809L -DHASH_BLOOM=24 -Wall -Wextra -Wpedantic
LDFLAGS += -Wl,-z,relro -Wl,-z,now

${EXECUTABLE}: ${OBJECT_FILES}
	${CC} ${LDFLAGS} -o $@ $^

%.o: %.c ${HEADER_FILES}
	${CC} ${CFLAGS} -o $@ -c $<

install: ${EXECUTABLE}
	install -m 0755 ${EXECUTABLE} /usr/local/bin/

clean:
	rm -f ${OBJECT_FILES} ${EXECUTABLE}
