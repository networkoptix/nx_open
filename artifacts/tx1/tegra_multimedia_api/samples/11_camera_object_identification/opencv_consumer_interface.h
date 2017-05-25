/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _OPENCV_CONSUMER_INTERFACE_H
#define _OPENCV_CONSUMER_INTERFACE_H

#define OPENCV_HANDLER_OPEN_ENTRY "opencv_handler_open"
typedef void *(*opencv_handler_open_t) (void);
#define OPENCV_HANDLER_CLOSE_ENTRY "opencv_handler_close"
typedef void (*opencv_handler_close_t) (void *);
#define OPENCV_IMG_PROCESSING_ENTRY "opencv_img_processing"
typedef void (*opencv_img_processing_t) (void *, const uint8_t *, int32_t, int32_t);
#define OPENCV_SET_CONFIG_ENTRY "opencv_set_config"
typedef void (*opencv_set_config_t) (void *, int, void *);

enum
{
    /* image width config, int */
    OPENCV_CONSUMER_CONFIG_IMGWIDTH = 0,
    /* image height config, int */
    OPENCV_CONSUMER_CONFIG_IMGHEIGHT,
    /* CAFFE model file path, string */
    OPENCV_CONSUMER_CONFIG_CAFFE_MODELFILE,
    /* CAFFE trained file path, string */
    OPENCV_CONSUMER_CONFIG_CAFFE_TRAINEDFILE,
    /* CAFFE mean file path, string */
    OPENCV_CONSUMER_CONFIG_CAFFE_MEANFILE,
    /* CAFFE label file path, string */
    OPENCV_CONSUMER_CONFIG_CAFFE_LABELFILE,
    /* opencv consumer start, NULL */
    OPENCV_CONSUMER_CONFIG_START
};

#endif  /* #ifndef _OPENCV_CONSUMER_INTERFACE_H */
