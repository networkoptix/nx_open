NX_KIT = ../../../buildenv/packages/any/nx_kit

TARGET = libproxydecoder.so
SRC = vdpau_utils.cpp vdpau_session.cpp proxy_decoder.cpp proxy_decoder_utils.cpp \
    proxy_decoder_impl.cpp proxy_decoder_stub.cpp \
    $(NX_KIT)/src/nx/kit/ini_config.cpp $(NX_KIT)/src/nx/kit/debug.cpp
CXXFLAGS ?= -Wall -Wno-sign-compare -Wfatal-errors -O3 -std=c++11
LDFLAGS ?= -fvisibility=hidden
LIBS = -L/opt/networkoptix/lib -lavcodec -lavfilter -lavformat -lavutil -lavdevice -lswscale \
	-lvdpau -lcedrus -lpixman-1
CXX ?= g++

DEP_CFLAGS = -MD -MP -MQ $@
LIB_CFLAGS = -fPIC
LIB_LDFLAGS = -shared -Wl,-soname,$(TARGET)

# Allows ProxyDecoder to use its own ffmpeg located in "lib/ffmpeg".
LIB_LDFLAGS += -Wl,-rpath,'$$ORIGIN/ffmpeg'

OBJ = $(addsuffix .o,$(basename $(SRC)))
DEP = $(addsuffix .d,$(basename $(SRC)))

MODULEDIR=/opt/networkoptix/lib
NX_FLAGS = \
    -I$(NX_KIT)/src \
    -DNX_KIT_API= \
    -DPROXY_DECODER_API='__attribute__((visibility ("default")))' -fvisibility=hidden \

.PHONY: clean all build install uninstall

all: install
build: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LIB_LDFLAGS) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(TARGET)

install: $(TARGET)
	install -D $(TARGET) $(DESTDIR)$(MODULEDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(MODULEDIR)/$(TARGET)

%.o: %.cpp
	$(CXX) $(DEP_CFLAGS) $(LIB_CFLAGS) $(CXXFLAGS) $(NX_FLAGS) -c $< -o $@

include $(wildcard $(DEP))
