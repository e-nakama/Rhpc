include ${MKCONF}

MPI_CFLAGS = -I/usr/lib/x86_64-linux-gnu/openmpi/include -I/usr/lib/x86_64-linux-gnu/openmpi/include/openmpi

SOURCES_C = RhpcSerialize.c RhpcMS.c
OBJECTS = $(SOURCES_C:.c=.o)

all: $(OBJECTS)

.c.o:
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(MPI_CFLAGS) -I. -c $< -o $@

clean:
	rm -f $(OBJECTS)
