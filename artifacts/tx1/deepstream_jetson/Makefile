################################################################################
# Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
################################################################################

CXX := g++

SRCS := gstdsexample.cpp $(wildcard dsopenalpr_lib/*.cpp)
INCS:= $(wildcard *.h) $(wildcard dsopenalpr_lib/*.h)
LIB:=libgstnvdsexample.so


CFLAGS := -fPIC -std=c++11
LIBS := -shared -L/usr/lib/aarch64-linux-gnu/tegra -lnvbuf_utils -lgstnvivameta \
  -Wl,-no-undefined -lEGL \
  -lopenalpr  -lvehicleclassifier -lalprstream 



CFLAGS+= \
  -I../nvgstiva-app_sources/nvgstiva-app/includes 

OBJS:= $(SRCS:.cpp=.o)

PKGS:= gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0 opencv
CFLAGS+=$(shell pkg-config --cflags $(PKGS))
LIBS+=$(shell pkg-config --libs $(PKGS))

all: $(LIB)

%.o: %.cpp $(INCS) Makefile
	@echo $(CFLAGS)
	$(CXX) -c -o $@ $(CFLAGS) $<

$(LIB): $(OBJS) Makefile
	@echo $(CFLAGS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

$(DEP): $(DEP_FILES)
	$(MAKE) -C dsopenalpr_lib/

install: $(LIB)
	cp -rv $(LIB) /usr/lib/aarch64-linux-gnu/gstreamer-1.0/

clean:
	rm -rf $(OBJS) $(LIB)
