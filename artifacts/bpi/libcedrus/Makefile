TARGET = libcedrus.so.1
SRC = cedrus.c cedrus_mem_ve.c cedrus_mem_ion.c
INC = cedrus.h cedrus_regs.h
CFLAGS ?= -Wall -Wextra -O3
LDFLAGS ?=
LIBS = -lpthread
CC ?= gcc

prefix ?= /usr/local
libdir ?= $(prefix)/lib
includedir ?= $(prefix)/include

ifeq ($(USE_UMP),1)
SRC += cedrus_mem_ump.c
CFLAGS += -DUSE_UMP
LIBS += -lUMP
endif

DEP_CFLAGS = -MD -MP -MQ $@
LIB_CFLAGS = -fpic -fvisibility=hidden
LIB_LDFLAGS = -shared -Wl,-soname,$(TARGET)

OBJ = $(addsuffix .o,$(basename $(SRC)))
DEP = $(addsuffix .d,$(basename $(SRC)))

.PHONY: clean all install uninstall

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(LIB_LDFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(TARGET)

install: $(TARGET) $(INC)
	install -D $(TARGET) $(DESTDIR)/$(libdir)/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)/$(libdir)/$(basename $(TARGET))
	install -D -t $(DESTDIR)/$(includedir)/cedrus/ $(INC)

uninstall:
	rm -f $(DESTDIR)/$(libdir)/$(basename $(TARGET))
	rm -f $(DESTDIR)/$(libdir)/$(TARGET)
	rm -rf $(DESTDIR)/$(includedir)/cedrus

%.o: %.c
	$(CC) $(DEP_CFLAGS) $(LIB_CFLAGS) $(CFLAGS) -c $< -o $@

include $(wildcard $(DEP))
