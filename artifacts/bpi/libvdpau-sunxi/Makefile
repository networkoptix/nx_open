NX_KIT = ../../../buildenv/packages/any/nx_kit

TARGET = libvdpau_sunxi.so.1
SRC = vdpau_private.c device.c presentation_queue.c surface_output.c surface_video.c \
	surface_bitmap.c video_mixer.c decoder.c handles.c \
	h264.c mpeg12.c mpeg4.c rgba.c tiled_yuv.S h265.c sunxi_disp.c \
	sunxi_disp2.c sunxi_disp1_5.c rgba_g2d.c rgba_pixman.c
SRC_CPP = $(NX_KIT)/src/nx/kit/ini_config.cpp ini.cpp
CFLAGS ?= -Wall -O3
LDFLAGS ?=
#LIBS = -lrt -lm -lX11 -lpthread -lcedrus
LIBS = -lrt -lm -lpthread -lcedrus
CC ?= cc

CFLAGS += -std=gnu99 $(shell pkg-config --cflags pixman-1)
CXXFLAGS += -fPIC -std=c++11
LIBS += $(shell pkg-config --libs pixman-1)

DEP_CFLAGS = -MD -MP -MQ $@
LIB_CFLAGS = -fPIC -fvisibility=hidden
LIB_LDFLAGS = -shared -Wl,-soname,$(TARGET)

OBJ = $(addsuffix .o,$(basename $(SRC))) $(addsuffix .o,$(basename $(SRC_CPP)))
DEP = $(addsuffix .d,$(basename $(SRC))) $(addsuffix .d,$(basename $(SRC_CPP)))

#MODULEDIR = $(shell pkg-config --variable=moduledir vdpau)

ifeq ($(MODULEDIR),)
MODULEDIR=/usr/lib/arm-linux-gnueabihf/vdpau
endif

.PHONY: clean all install uninstall build

NX_KIT_FLAGS = \
    -DNX_KIT_API='__attribute__((visibility ("hidden")))' \
    -I$(NX_KIT)/src \

all: install
build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LIB_LDFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(TARGET)

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)$(MODULEDIR)/$(TARGET)
	ln -sf $(TARGET) $(DESTDIR)$(MODULEDIR)/$(basename $(TARGET))

uninstall:
	rm -f $(DESTDIR)$(MODULEDIR)/$(basename $(TARGET))
	rm -f $(DESTDIR)$(MODULEDIR)/$(TARGET)

%.o: %.cpp
	$(CXX) $(DEP_CFLAGS) $(CXXFLAGS) $(NX_KIT_FLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(DEP_CFLAGS) $(LIB_CFLAGS) $(CFLAGS) $(NX_KIT_FLAGS) -c $< -o $@

%.o: %.S
	$(CC) -c $< -o $@

include $(wildcard $(DEP))
