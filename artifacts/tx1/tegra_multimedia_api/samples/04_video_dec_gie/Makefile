###############################################################################
#
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################

export TEGRA_ARMABI = aarch64-linux-gnu

include ../Rules.mk

APP := video_dec_gie
SO := libtegra_video.so
TEST := unit_tests/net_dimensions_ut

CLASS_DIR := ../common/classes
NX_KIT_DIR := ../../../../../open/artifacts/nx_kit/src
ALGO_DIR := ../common/algorithm/cuda
GIE_DIR := ../common/algorithm/gie

HEADERS := $(wildcard *.h) #< For simplicity, dependency on external headers is not defined.

# Allows to add flags in command line: make ADDITIONAL_CPPFLAGS=...
CPPFLAGS += \
    $(ADDITIONAL_CPPFLAGS) \
    -std=c++11 \
    -fPIC \
    -fvisibility=hidden \
    -DNX_KIT_API= \
    -DTEGRA_VIDEO_API='__attribute__ ((visibility ("default")))' \
    -I"." \
    -I"$(ALGO_DIR)" \
    -I"$(GIE_DIR)" \
    -I"$(CUDA_PATH)/targets/aarch64-linux/include" \
    -I"$(NX_KIT_DIR)" \

LDFLAGS += \
    -L"$(CUDA_PATH)/targets/aarch64-linux/lib" \
    -ldl -lpthread -lrt \
    -lv4l2 -lEGL -lGLESv2 -lX11 \
    -lnvbuf_utils -lnvjpeg \
    -lcuda -lcudart \

NX_KIT_OBJS := \
    nx_kit-build/debug.o \
    nx_kit-build/ini_config.o \
    nx_kit-build/test.o \

SRCS := \
    video_dec_gie_csvparser.cpp \
    video_dec_gie_main.cpp \
    main_tv.cpp \
    tegra_video.cpp \
    tegra_video_impl.cpp \
    tegra_video_stub.cpp \
    net_dimensions.cpp \
    rects_serialization.cpp \
    $(wildcard $(CLASS_DIR)/*.cpp) \

OBJS := $(SRCS:.cpp=.o)
OBJS += \
    $(ALGO_DIR)/NvAnalysis.o \
    $(ALGO_DIR)/NvCudaProc.o \
    $(GIE_DIR)/gie_inference.o \
    $(NX_KIT_OBJS) \

LDFLAGS += \
    -lnvinfer \
    -lnvcaffe_parser \
    -lcudnn \
    -lcublas \
    -lcudart_static \
    -lnvToolsExt \
    -lrt \
    -ldl \
    -lwconv \
    -lopencv_core \
    -lopencv_imgproc \
    -lopencv_highgui \
    -lopencv_objdetect \

#--------------------------------------------------------------------------------------------------

.PHONY: all
all: $(APP) $(SO) $(TEST)

.PHONY: mostlyclean
mostlyclean:; \
    rm -f *.o unit_tests/*.o $(APP) $(SO) $(TEST)

.PHONY: clean
clean: mostlyclean; \
    rm -f $(NX_KIT_OBJS); \
    $(MAKE) -C $(CLASS_DIR) clean; \
    $(MAKE) -C $(ALGO_DIR) clean; \
    $(MAKE) -C $(GIE_DIR) clean; \

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp; \
    $(MAKE) -C $(CLASS_DIR)

$(ALGO_DIR)/%.o: $(ALGO_DIR)/%.cpp; \
    $(MAKE) -C $(ALGO_DIR)

$(ALGO_DIR)/%.o: $(ALGO_DIR)/%.cu; \
    $(MAKE) -C $(ALGO_DIR)

$(GIE_DIR)/%.o: $(GIE_DIR)/%.cpp; \
    $(MAKE) -C $(GIE_DIR)

nx_kit-build/%.o: $(NX_KIT_DIR)/nx/kit/%.cpp $(wildcard $(NX_KIT_DIR)/nx/kit/*.h); \
    mkdir -p nx_kit-build; \
    $(CPP) -o $@ $(CPPFLAGS) -c $<

%.o: %.cpp $(HEADERS); \
    $(CPP) -o $@ $(CPPFLAGS) -c $<

unit_tests/%.o: unit_tests/%.cpp $(HEADERS); \
    $(CPP) -o $@ $(CPPFLAGS) -c $<

$(APP): $(OBJS); \
    $(CPP) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS)

$(SO): $(APP); \
    $(CPP) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS) -shared

TEST_OBJS := \
    net_dimensions.o \
    unit_tests/net_dimensions_ut.o \
    $(NX_KIT_OBJS) \

$(TEST): $(TEST_OBJS); \
    $(CPP) -o $@ $(TEST_OBJS) $(CPPFLAGS) $(LDFLAGS)
