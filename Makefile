CFLAGS=-O2 -I../influxdb-cpp
EXEC = hgi80
SOURCES = $(wildcard *.cc)
OBJECTS = $(SOURCES:.cc=.o)
LIBS=-lpthread
CC=g++

$(EXEC):	$(OBJECTS)
	$(CC) -o $(EXEC) $(OBJECTS) ${LIBS}

%.o: %.cc
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(EXEC) $(OBJECTS)
