CC ?= gcc
CFLAGS += -g -O0 -DDEBUG
LDFLAGS += -L. -lpthread

.PHONY: clean all

TARGETS =   main test_move test_move2 test_max

LIB_OBJ = 	buflib.o \
			new_apis.o \
			core_api.o
LIB_FILE = libbuflib.a
LIB = buflib

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

main: main.o $(LIB)
	$(CC) $(LDFLAGS) $@.o -o $@ -l$(LIB)

test_move: test_move.o $(LIB)
	$(CC) $(LDFLAGS) $@.o -o $@ -l$(LIB)

test_move2: test_move2.o $(LIB)
	$(CC) $(LDFLAGS) $@.o -o $@ -l$(LIB)

test_max: test_max.o $(LIB)
	$(CC) $(LDFLAGS) $@.o -o $@ -l$(LIB)

$(LIB): $(LIB_FILE)
	$(echo Hey)

$(LIB_FILE): $(LIB_OBJ)
	ar rcs $@ $^

clean:
	rm *.o $(TARGETS) $(LIB_FILE)
