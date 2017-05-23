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

#include "NvUtils.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include "video_convert.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

using namespace std;

static void
abort(context_t * ctx)
{
    ctx->got_error = true;
    ctx->conv0->abort();
    if (ctx->conv1)
    {
        ctx->conv1->abort();
    }
    pthread_cond_broadcast(&ctx->queue_cond);
}
static bool
conv0_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvBuffer *conv1_buffer;
    struct v4l2_buffer conv1_qbuf;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv0 capture plane" << endl;
        abort(ctx);
        return false;
    }

    if (ctx->in_buftype ==  BUF_TYPE_NVBL || ctx->out_buftype == BUF_TYPE_NVBL)
    {
        // Get an empty conv1 output plane buffer from conv1_output_plane_buf_queue
        pthread_mutex_lock(&ctx->queue_lock);
        while (ctx->conv1_output_plane_buf_queue->empty() && !ctx->got_error)
        {
            pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
        }

        if (ctx->got_error)
        {
            pthread_mutex_unlock(&ctx->queue_lock);
            return false;
        }
        conv1_buffer = ctx->conv1_output_plane_buf_queue->front();
        ctx->conv1_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx->queue_lock);

        memset(&conv1_qbuf, 0, sizeof(conv1_qbuf));
        memset(&planes, 0, sizeof(planes));

        conv1_qbuf.index = conv1_buffer->index;
        conv1_qbuf.m.planes = planes;

        // A reference to buffer is saved which can be used when
        // buffer is dequeued from conv1 output plane
        if (ctx->conv1->output_plane.qBuffer(conv1_qbuf, buffer)  < 0)
        {
            cerr << "Error queueing buffer on conv1 output plane" << endl;
            abort(ctx);
            return false;
        }

        if (v4l2_buf->m.planes[0].bytesused == 0)
        {
            return false;
        }
    }
    else
    {
        if (v4l2_buf->m.planes[0].bytesused == 0)
        {
            return false;
        }
        write_video_frame(ctx->out_file, *buffer);
        if (ctx->conv0->capture_plane.qBuffer(*v4l2_buf, buffer) < 0)
        {
            cerr << "Error queueing buffer on conv0 capture plane" << endl;
            abort(ctx);
            return false;
        }
    }
    return true;
}

static bool
conv1_output_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                  NvBuffer * buffer, NvBuffer * shared_buffer,
                                  void *arg)
{
    context_t *ctx = (context_t *) arg;
    struct v4l2_buffer conv0_ret_qbuf;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv1 output plane" << endl;
        abort(ctx);
        return false;
    }

    memset(&conv0_ret_qbuf, 0, sizeof(conv0_ret_qbuf));
    memset(&planes, 0, sizeof(planes));

    // Get the index of the conv0 capture plane shared buffer
    conv0_ret_qbuf.index = shared_buffer->index;
    conv0_ret_qbuf.m.planes = planes;

    // Add the dequeued buffer to conv1 empty output buffers queue to
    // conv1_output_plane_buf_queue
    // queue the shared buffer back in conv0 capture plane
    pthread_mutex_lock(&ctx->queue_lock);
    if (ctx->conv0->capture_plane.qBuffer(conv0_ret_qbuf, NULL) < 0)
    {
        cerr << "Error queueing buffer on conv0 capture plane" << endl;
        abort(ctx);
        return false;
    }
    ctx->conv1_output_plane_buf_queue->push(buffer);
    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }
    return true;
}

static bool
conv1_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv1 capture plane" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }
    write_video_frame(ctx->out_file, *buffer);
    if (ctx->conv1->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
    {
        cerr << "Error queueing buffer on conv1 capture plane" << endl;
        abort(ctx);
        return false;
    }
    return true;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->conv1_output_plane_buf_queue = new queue < NvBuffer * >;
    pthread_mutex_init(&ctx->queue_lock, NULL);
    pthread_cond_init(&ctx->queue_cond, NULL);

    ctx->interpolation_method = (enum v4l2_interpolation_method) -1;
    ctx->tnr_algorithm = (enum v4l2_tnr_algorithm) -1;
    ctx->flip_method = (enum v4l2_flip_method) -1;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    NvVideoConverter *main_conv;
    int ret = 0;
    int error = 0;
    bool eos = false;

    set_defaults(&ctx);

    ret = parse_csv_args(&ctx, argc, argv);
    TEST_ERROR(ret < 0, "Error parsing commandline arguments", cleanup);

    TEST_ERROR(ctx.in_buftype == BUF_TYPE_NVBL &&
            ctx.out_buftype == BUF_TYPE_NVBL,
            "NV BL -> NV BL conversions are not supported", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Could not open input file", cleanup);

    ctx.out_file = new ofstream(ctx.out_file_path);
    TEST_ERROR(!ctx.out_file->is_open(), "Could not open output file", cleanup);

    ctx.conv0 = NvVideoConverter::createVideoConverter("conv0");
    TEST_ERROR(!ctx.conv0, "Could not create Video Converter", cleanup);

    if (ctx.in_buftype == BUF_TYPE_NVBL || ctx.out_buftype == BUF_TYPE_NVBL)
    {
        ctx.conv1 = NvVideoConverter::createVideoConverter("conv1");
        TEST_ERROR(!ctx.conv1, "Could not create Video Converter", cleanup);
    }

    if (ctx.in_buftype == BUF_TYPE_NVBL)
    {
        main_conv = ctx.conv1;
    }
    else
    {
        main_conv = ctx.conv0;
    }

    if (ctx.flip_method != -1)
    {
        ret = main_conv->setFlipMethod(ctx.flip_method);
        TEST_ERROR(ret < 0, "Could not set flip method", cleanup);
    }

    if (ctx.interpolation_method != -1)
    {
        ret = main_conv->setInterpolationMethod(ctx.interpolation_method);
        TEST_ERROR(ret < 0, "Could not set interpolation method", cleanup);
    }

    if (ctx.tnr_algorithm != -1)
    {
        ret = main_conv->setTnrAlgorithm(ctx.tnr_algorithm);
        TEST_ERROR(ret < 0, "Could not set tnr algorithm", cleanup);
    }

    if (ctx.crop_rect.width != 0 && ctx.crop_rect.height != 0)
    {
        ret = main_conv->setCropRect(ctx.crop_rect.left, ctx.crop_rect.top,
                ctx.crop_rect.width, ctx.crop_rect.height);
        TEST_ERROR(ret < 0, "Could not set crop rect for conv0",
                cleanup);
    }

    // Set conv0 output plane format
    ret = ctx.conv0->setOutputPlaneFormat(ctx.in_pixfmt, ctx.in_width,
                ctx.in_height, V4L2_NV_BUFFER_LAYOUT_PITCH);
    TEST_ERROR(ret < 0, "Could not set output plane format for conv0", cleanup);

    if (ctx.in_buftype == BUF_TYPE_NVBL)
    {
    // Set conv0 capture plane format
    ret = ctx.conv0->setCapturePlaneFormat(ctx.in_pixfmt, ctx.in_width,
                ctx.in_height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    }
    else if (ctx.out_buftype == BUF_TYPE_NVBL)
    {
    ret = ctx.conv0->setCapturePlaneFormat(ctx.out_pixfmt, ctx.out_width,
                ctx.out_height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    }
    else
    {
    ret = ctx.conv0->setCapturePlaneFormat(ctx.out_pixfmt, ctx.out_width,
                ctx.out_height, V4L2_NV_BUFFER_LAYOUT_PITCH);
    }
    TEST_ERROR(ret < 0, "Could not set capture plane format for conv0", cleanup);

    if (ctx.in_buftype == BUF_TYPE_NVBL)
    {
    // Set conv1 output plane format
    ret =
        ctx.conv1->setOutputPlaneFormat(ctx.in_pixfmt, ctx.in_width,
                                       ctx.in_height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    }
    else if (ctx.out_buftype == BUF_TYPE_NVBL)
    {
    ret =
        ctx.conv1->setOutputPlaneFormat(ctx.out_pixfmt, ctx.out_width,
                                       ctx.out_height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    }
    TEST_ERROR(ret < 0, "Could not set output plane format for conv1", cleanup);

    if (ctx.conv1)
    {
        // Set conv1 capture plane format
        ret =
            ctx.conv1->setCapturePlaneFormat(ctx.out_pixfmt, ctx.out_width,
                    ctx.out_height, V4L2_NV_BUFFER_LAYOUT_PITCH);
        TEST_ERROR(ret < 0, "Could not set capture plane format for conv1",
                cleanup);
    }

    // REQBUF, EXPORT and MAP conv0 output plane buffers
    if (ctx.in_buftype == BUF_TYPE_RAW)
    {
    ret = ctx.conv0->output_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false, true);
    }
    else
    {
    ret = ctx.conv0->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    }
    TEST_ERROR(ret < 0, "Error while setting up output plane for conv0",
               cleanup);

    // REQBUF and EXPORT conv0 capture plane buffers
    // No need to MAP since buffer will be shared to next component
    // and not read in application
    if (ctx.out_buftype == BUF_TYPE_RAW && ctx.in_buftype != BUF_TYPE_NVBL)
    {
    ret =
        ctx.conv0->capture_plane.setupPlane(V4L2_MEMORY_USERPTR, 10, false, true);
    }
    else if (ctx.in_buftype == BUF_TYPE_NVBL || ctx.out_buftype == BUF_TYPE_NVBL)
    {
    ret = ctx.conv0->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 10, false, false);
    }
    else
    {
    ret =
        ctx.conv0->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    }
    TEST_ERROR(ret < 0, "Error while setting up capture plane for conv0",
               cleanup);

    // conv0 output plane STREAMON
    ret = ctx.conv0->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane streamon for conv0", cleanup);

    // conv0 capture plane STREAMON
    ret = ctx.conv0->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in capture plane streamon for conv0", cleanup);

    ctx.conv0->
        capture_plane.setDQThreadCallback(conv0_capture_dqbuf_thread_callback);

    if (ctx.conv1)
    {
        // REQBUF on conv1 output plane buffers
        // DMABUF is used here since it is a shared buffer allocated but another
        // component
        ret = ctx.conv1->output_plane.setupPlane(V4L2_MEMORY_DMABUF, 10, false,
                    false);
        TEST_ERROR(ret < 0, "Error while setting up output plane for conv1",
                cleanup);

        // REQBUF on conv1 capture plane buffers,
        // Since USERPTR is used, memory is allocated by the application
        if (ctx.out_buftype == BUF_TYPE_RAW)
        {
            ret = ctx.conv1->capture_plane.setupPlane(V4L2_MEMORY_USERPTR, 10,
                        false, true);
        }
        else
        {
            ret = ctx.conv1->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 10,
                    true, false);
        }
        TEST_ERROR(ret < 0, "Error while setting up capture plane for conv1",
                cleanup);

        // conv1 output plane STREAMON
        ret = ctx.conv1->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in output plane streamon for conv1", cleanup);

        // conv1 capture plane STREAMON
        ret = ctx.conv1->capture_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in capture plane streamon for conv1", cleanup);

        ctx.conv1->output_plane.setDQThreadCallback(
                conv1_output_dqbuf_thread_callback);
        ctx.conv1->capture_plane.setDQThreadCallback(
                conv1_capture_dqbuf_thread_callback);
    }

    // Start threads to dequeue buffers on conv0 capture plane,
    // conv1 output plane and conv1 capture plane
    ctx.conv0->capture_plane.startDQThread(&ctx);
    if (ctx.conv1)
    {
        ctx.conv1->output_plane.startDQThread(&ctx);
        ctx.conv1->capture_plane.startDQThread(&ctx);
    }

    // Enqueue all empty conv0 capture plane buffers
    for (uint32_t i = 0; i < ctx.conv0->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = ctx.conv0->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv0 capture plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    if (ctx.conv1)
    {
        // Add all empty conv1 output plane buffers to conv1_output_plane_buf_queue
        for (uint32_t i = 0; i < ctx.conv1->output_plane.getNumBuffers(); i++)
        {
            ctx.conv1_output_plane_buf_queue->push(ctx.conv1->output_plane.
                    getNthBuffer(i));
        }

        // Enqueue all empty conv1 capture plane buffers
        for (uint32_t i = 0; i < ctx.conv1->capture_plane.getNumBuffers(); i++)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

            v4l2_buf.index = i;
            v4l2_buf.m.planes = planes;

            ret = ctx.conv1->capture_plane.qBuffer(v4l2_buf, NULL);
            if (ret < 0)
            {
                cerr << "Error while queueing buffer at conv1 capture plane"
                    << endl;
                abort(&ctx);
                goto cleanup;
            }
        }
    }

    // Read video frame from file and queue buffer on conv0 output plane
    for (uint32_t i = 0; i < ctx.conv0->output_plane.getNumBuffers() &&
            !ctx.got_error && !eos; i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer = ctx.conv0->output_plane.getNthBuffer(i);

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        if (read_video_frame(ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            cerr << "File read complete." << endl;
            v4l2_buf.m.planes[0].bytesused = 0;
            eos = true;
        }

        ret = ctx.conv0->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv0 output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    // Read video frame from file till EOS is reached and queue buffer on conv0 output plane
    while (!ctx.got_error && !ctx.conv0->isInError() &&
            (!ctx.conv1 || !ctx.conv1->isInError()) && !eos)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        if (ctx.conv0->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, 100) < 0)
        {
            cerr << "ERROR while DQing buffer at conv0 output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }

        if (read_video_frame(ctx.in_file, *buffer) < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            cerr << "File read complete." << endl;
            v4l2_buf.m.planes[0].bytesused = 0;
            eos = true;
        }

        ret = ctx.conv0->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv0 output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    if (!ctx.got_error)
    {
        // Wait till all capture plane buffers on conv0 and conv1 are dequeued
        ctx.conv0->waitForIdle(-1);
        if (ctx.conv1)
        {
            ctx.conv1->waitForIdle(-1);
        }
    }

cleanup:
    if (ctx.conv0 && ctx.conv0->isInError())
    {
        cerr << "VideoConverter0 is in error" << endl;
        error = 1;
    }

    if (ctx.conv1 && ctx.conv1->isInError())
    {
        cerr << "VideoConverter1 is in error" << endl;
        error = 1;
    }

    if (ctx.got_error)
    {
        error = 1;
    }

    delete ctx.conv1_output_plane_buf_queue;
    delete ctx.in_file;
    delete ctx.out_file;
    // Destructors do all the cleanup, unmapping and deallocating buffers
    // and calling v4l2_close on fd
    delete ctx.conv0;
    delete ctx.conv1;

    free(ctx.in_file_path);
    free(ctx.out_file_path);

    if (error)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }
    return -error;
}
