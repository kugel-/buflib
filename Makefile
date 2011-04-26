CC ?= gcc
CFLAGS += -g -O -DDEBUG

TARGET = buflib

OBJ = 	buflib.o \
		main.o


all: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.o $(TARGET)
