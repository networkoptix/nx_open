TARGET = libproxydecoder.so
SRC = flag_config.cpp vdpau_helper.cpp proxy_decoder_utils.cpp \
	proxy_decoder_impl.cpp proxy_decoder_stub.cpp
CXXFLAGS ?= -Wall -Wno-sign-compare -Wfatal-errors -O3 -std=c++11
LDFLAGS ?=
LIBS = -L/opt/ffmpeg/lib -lavcodec -lavfilter -lavformat -lavutil -lavdevice -lswscale \
	-lvdpau -lcedrus -lpixman-1
CXX ?= g++

DEP_CFLAGS = -MD -MP -MQ $@
LIB_CFLAGS = -fPIC
LIB_LDFLAGS = -shared -Wl,-soname,$(TARGET)

OBJ = $(addsuffix .o,$(basename $(SRC)))
DEP = $(addsuffix .d,$(basename $(SRC)))

MODULEDIR=/opt/networkoptix/lite_client/lib
CONF_DEFINES = -DNX_UTILS_API='__attribute__ ((visibility ("hidden")))'

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
	$(CXX) $(DEP_CFLAGS) $(LIB_CFLAGS) $(CXXFLAGS) $(CONF_DEFINES) -c $< -o $@

include $(wildcard $(DEP))
