##
# Copyright (c) 2016, NVIDIA Corporation. All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto. Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
##

# TODO support cross-compilation

CC=g++
#CFLAGS+=-g

CFLAGS+= -fPIC -Wall -Wextra -O2 -g

CFLAGS+=`pkg-config --cflags opencv`
LDFLAGS+=`pkg-config --libs opencv`

CUDA_DIR:=/usr/local/cuda
CAFFE_DIR:=/home/ubuntu/Work/caffe/caffe-master
CFLAGS+=-I$(CAFFE_DIR)/include -I$(CUDA_DIR)/include -I$(CAFFE_DIR)/build/src
LDFLAGS+=-L$(CAFFE_DIR)/build/lib

TARGET_LIB=libopencv_consumer.so

LIB_SRCS=opencv_consumer.cpp Classifier.cpp
LIB_OBJS=$(LIB_SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET_LIB)

$(TARGET_LIB): $(LIB_OBJS)
	$(CC) -o $(TARGET_LIB) $(LIB_OBJS) $(LDFLAGS) -shared -lcaffe

%.o: %.cpp
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f $(LIB_OBJS) $(TARGET_LIB)
