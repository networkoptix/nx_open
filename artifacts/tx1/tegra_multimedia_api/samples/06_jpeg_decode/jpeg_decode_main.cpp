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

#include "jpeg_decode.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

using namespace std;

static void
abort(context_t * ctx)
{
    ctx->got_error = true;
    ctx->conv->abort();
}

static bool
conv_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv capture plane" << endl;
        abort(ctx);
        return false;
    }

    write_video_frame(ctx->out_file, *buffer);
    return false;
}

static uint64_t
get_file_size(ifstream * stream)
{
    uint64_t size = 0;
    streampos current_pos = stream->tellg();
    stream->seekg(0, stream->end);
    size = stream->tellg();
    stream->seekg(current_pos, stream->beg);
    return size;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->use_fd = true;
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;
    int error = 0;
    int fd;
    uint32_t width, height, pixfmt;

    set_defaults(&ctx);

    ret = parse_csv_args(&ctx, argc, argv);
    TEST_ERROR(ret < 0, "Error parsing commandline arguments", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Could not open input file", cleanup);

    ctx.out_file = new ofstream(ctx.out_file_path);
    TEST_ERROR(!ctx.out_file->is_open(), "Could not open output file", cleanup);

    ctx.jpegdec = NvJPEGDecoder::createJPEGDecoder("jpegdec");
    TEST_ERROR(!ctx.jpegdec, "Could not create Jpeg Decoder", cleanup);

    ctx.in_file_size = get_file_size(ctx.in_file);
    ctx.in_buffer = new unsigned char[ctx.in_file_size];
    ctx.in_file->read((char *) ctx.in_buffer, ctx.in_file_size);

    if (!ctx.use_fd)
    {
        NvBuffer *buffer;
        ret = ctx.jpegdec->decodeToBuffer(&buffer, ctx.in_buffer,
                  ctx.in_file_size, &pixfmt, &width, &height);
        TEST_ERROR(ret < 0, "Could not decode image", cleanup);
        cout << "Image Resolution - " << width << " x " << height << endl;
        write_video_frame(ctx.out_file, *buffer);
        delete buffer;
        goto cleanup;
    }

    ctx.conv = NvVideoConverter::createVideoConverter("conv");
    TEST_ERROR(!ctx.conv, "Could not create Video Converter", cleanup);

    ret = ctx.jpegdec->decodeToFd(fd, ctx.in_buffer, ctx.in_file_size, pixfmt,
            width, height);
    TEST_ERROR(ret < 0, "Could not decode image", cleanup);
    cout << "Image Resolution - " << width << " x " << height << endl;

    ret = ctx.conv->setCropRect(0, 0, width, height);
    TEST_ERROR(ret < 0, "Could not set crop rect for conv0", cleanup);

    // Set conv output plane format
    ret =
        ctx.conv->setOutputPlaneFormat(pixfmt, width,
                height, V4L2_NV_BUFFER_LAYOUT_PITCH);
    TEST_ERROR(ret < 0, "Could not set output plane format for conv", cleanup);

    // Set conv capture plane format
    ret =
        ctx.conv->setCapturePlaneFormat(pixfmt, width,
                height,
                V4L2_NV_BUFFER_LAYOUT_PITCH);
    TEST_ERROR(ret < 0, "Could not set capture plane format for conv", cleanup);

    // REQBUF, EXPORT and MAP conv output plane buffers
    ret = ctx.conv->output_plane.setupPlane(V4L2_MEMORY_DMABUF, 1, false, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane for conv",
            cleanup);

    // REQBUF and EXPORT conv capture plane buffers
    // No need to MAP since buffer will be shared to next component
    // and not read in application
    ret =
        ctx.conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 1,
                true, false);
    TEST_ERROR(ret < 0, "Error while setting up capture plane for conv",
            cleanup);

    // conv output plane STREAMON
    ret = ctx.conv->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane streamon for conv", cleanup);

    // conv capture plane STREAMON
    ret = ctx.conv->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in capture plane streamon for conv", cleanup);

    ctx.conv->
        capture_plane.setDQThreadCallback(conv_capture_dqbuf_thread_callback);

    // Start threads to dequeue buffers on conv capture plane
    ctx.conv->capture_plane.startDQThread(&ctx);

    // Enqueue all empty conv capture plane buffers
    for (uint32_t i = 0; i < ctx.conv->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = ctx.conv->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv capture plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = 0;
        v4l2_buf.m.planes = planes;
        planes[0].m.fd = fd;
        planes[0].bytesused = 1234;

        ret = ctx.conv->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error while queueing buffer at conv output plane" << endl;
            abort(&ctx);
            goto cleanup;
        }
    }

    // Wait till all capture plane buffers on conv are dequeued
    ctx.conv->capture_plane.waitForDQThread(2000);

cleanup:
    if (ctx.conv && ctx.conv->isInError())
    {
        cerr << "VideoConverter is in error" << endl;
        error = 1;
    }

    if (ctx.got_error)
    {
        error = 1;
    }

    delete ctx.in_file;
    delete ctx.out_file;
    // Destructors do all the cleanup, unmapping and deallocating buffers
    // and calling v4l2_close on fd
    delete ctx.conv;
    delete ctx.jpegdec;
    delete[] ctx.in_buffer;

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
