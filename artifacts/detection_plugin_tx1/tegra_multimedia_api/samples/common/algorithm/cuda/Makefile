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

include ../../../Rules.mk

# Internal flags
NVCCFLAGS := --shared
CPPFLAGS += -fPIC

# Debug flags
ifneq ($(CUDA_DEBUG),)
	NVCCFLAGS += -g -G
endif

ALL_CPPFLAGS :=
ALL_CPPFLAGS += $(NVCCFLAGS)
ALL_CPPFLAGS += $(addprefix -Xcompiler ,$(CPPFLAGS))

# Common includes and paths for CUDA
INCLUDES  := -I"../../../../include"

# CUDA code generation flags
GENCODE_SM53 := -gencode arch=compute_53,code=sm_53
GENCODE_SM62 := -gencode arch=compute_62,code=sm_62
GENCODE_FLAGS := $(GENCODE_SM53) $(GENCODE_SM62)

# Target rules
all: NvAnalysis.o NvCudaProc.o

NvAnalysis.o : NvAnalysis.cu
	$(NVCC) $(INCLUDES) $(ALL_CPPFLAGS) $(GENCODE_FLAGS) -o $@ -c $<

NvCudaProc.o : NvCudaProc.cpp
	$(NVCC) $(INCLUDES) $(ALL_CPPFLAGS) $(GENCODE_FLAGS) -o $@ -c $<

clean:
	rm -rf *.o
