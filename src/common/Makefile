include ${MKCONF}

PKG_CPPFLAGS = -I. -I/usr/lib/x86_64-linux-gnu/openmpi/include -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi

SOURCES_C = RhpcSerialize.c RhpcMS.c
OBJECTS = $(SOURCES_C:.c=.o)

all: $(OBJECTS)

clean:
	rm -f $(OBJECTS)
