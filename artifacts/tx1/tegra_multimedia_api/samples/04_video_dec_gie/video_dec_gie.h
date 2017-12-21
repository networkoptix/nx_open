#pragma once

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

#include "NvVideoDecoder.h"
#include "NvVideoConverter.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include <queue>
#include <fstream>
#include <pthread.h>
#include "gie_inference.h"
#include "nvosd.h"
#include <vector>
#include <queue>
#include <opencv2/objdetect.hpp>
#include <chrono>

using namespace std;

typedef struct
{
    struct v4l2_buffer v4l2_buf;
    NvBuffer           *buffer;
    NvBuffer           *shared_buffer;
    void               *arg;
    int                bProcess;
} Shared_Buffer;

typedef struct
{
    NvVideoDecoder *dec;
    NvVideoConverter *conv;
    uint32_t decoder_pixfmt;

    char *in_file_path;
    std::ifstream *in_file;

    EGLDisplay egl_display;

    queue<Shared_Buffer> *gie_buf_queue;
    pthread_mutex_t       gie_lock; // for dec and conv
    pthread_cond_t        gie_cond;
    int                   gie_stop;
    pthread_t             gie_thread_handle;
    GIE_Context          *gie_ctx;

    bool disable_dpb;
    uint32_t dec_width;
    uint32_t dec_height;

    std::queue < NvBuffer * > *conv_output_plane_buf_queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;

    pthread_t dec_capture_loop;
    bool got_error;
    bool got_eos;

    string deployfile;
    string modelfile;
    string cachefile;
    std::queue<std::vector<cv::Rect>>* rectQueuePtr;
    bool needToStop;
    std::chrono::high_resolution_clock::time_point m_firstFrameArrivalTime
        = std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(0));
    int m_framesProcessed = 0;

    std::chrono::high_resolution_clock::time_point m_lastInferenceTime
        = std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(0));

    std::chrono::microseconds m_lastProcessedFrameTimestamp;
    std::chrono::microseconds m_lastInferenceDuration;

    std::queue<int64_t> m_ptsQueue;
    std::queue<int64_t> m_outPtsQueue;

} context_t;

typedef struct
{
    uint32_t window_height;
    uint32_t window_width;
}display_resolution_t;

class GIE_Context;

int parseCsvArgs(context_t * ctx, GIE_Context *gie_ctx, int argc, char *argv[]);
