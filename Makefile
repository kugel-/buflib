CC ?= gcc
CFLAGS += -g -O0 -DDEBUG

TARGET = buflib

OBJ = 	buflib.o \
		new_apis.o \
		main.o


all: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.o $(TARGET)
