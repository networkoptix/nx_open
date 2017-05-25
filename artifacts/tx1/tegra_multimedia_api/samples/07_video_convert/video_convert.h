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

#include "NvVideoConverter.h"
#include <fstream>
#include <queue>
#include <pthread.h>

#define BUF_TYPE_NVPL 0
#define BUF_TYPE_NVBL 1
#define BUF_TYPE_RAW 2

typedef struct
{
    NvVideoConverter *conv0;
    NvVideoConverter *conv1;

    char *in_file_path;
    std::ifstream * in_file;
    uint32_t in_width;
    uint32_t in_height;
    uint32_t in_pixfmt;
    uint32_t in_buftype;

    char *out_file_path;
    std::ofstream * out_file;
    uint32_t out_width;
    uint32_t out_height;
    uint32_t out_pixfmt;
    uint32_t out_buftype;

    std::queue < NvBuffer * > *conv1_output_plane_buf_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;

    enum v4l2_flip_method flip_method;
    enum v4l2_interpolation_method interpolation_method;
    enum v4l2_tnr_algorithm tnr_algorithm;

    struct v4l2_rect crop_rect;

    bool got_error;
} context_t;

int parse_csv_args(context_t * ctx, int argc, char *argv[]);
