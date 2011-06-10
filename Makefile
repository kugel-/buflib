CC ?= gcc
CFLAGS += -g -O0 -std=gnu99
LDFLAGS += -L. -lpthread

.PHONY: clean all

TARGETS_OBJ = test_main.o   \
			  test_move.o   \
			  test_move2.o  \
			  test_max.o    \
			  test_shrink.o \
			  test_shrink_unaligned.o \
			  test_shrink_startchanged.o \
			  test_shrink_cb.o
TARGETS = $(TARGETS_OBJ:.o=)

LIB_OBJ = 	buflib.o \
			new_apis.o \
			core_api.o \
			strlcpy.o
LIB_FILE = libbuflib.a
LIB = buflib


ifndef V
SILENT:=@
else
VERBOSEOPT:=-v
endif

PRINTS=$(SILENT)$(call info,$(1))

all: $(TARGETS)

test_%: test_%.o $(LIB_FILE)
	$(call PRINTS,LD $@)$(CC) $(LDFLAGS) -o $@ $< -l$(LIB)

$(TARGETS): $(TARGETS_OBJ) $(LIB_FILE)

%.o: %.c
	$(call PRINTS,CC $<)$(CC) $(CFLAGS) -c $<

$(LIB_FILE): $(LIB_OBJ)
	$(call PRINTS,AR $@)ar rcs $@ $^


clean:
	rm *.o $(TARGETS) $(LIB_FILE)
