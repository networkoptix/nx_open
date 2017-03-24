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
#include "NvCudaProc.h"
#include "nvbuf_utils.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <string>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>

#include "videodec.h"
#include "nvosd.h"

#define TEST_ERROR(cond, str, label) if(cond) { \
                                        cerr << str << endl; \
                                        error = 1; \
                                        goto label; }

#define CHUNK_SIZE 4000000
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define IS_NAL_UNIT_START(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        !buffer_ptr[2] && (buffer_ptr[3] == 1))

#define IS_NAL_UNIT_START1(buffer_ptr) (!buffer_ptr[0] && !buffer_ptr[1] && \
        (buffer_ptr[2] == 1))

#define BORDER_WIDTH 5

using namespace std;

static int
read_decoder_input_nalu(ifstream * stream, NvBuffer * buffer,
        char *parse_buffer, streamsize parse_buffer_size)
{
    // Length is the size of the buffer in bytes
    char *buffer_ptr = (char *) buffer->planes[0].data;

    char *stream_ptr;
    bool nalu_found = false;

    streamsize bytes_read;
    streamsize stream_initial_pos = stream->tellg();

    stream->read(parse_buffer, parse_buffer_size);
    bytes_read = stream->gcount();

    if (bytes_read == 0)
    {
        return buffer->planes[0].bytesused = 0;
    }

    // Find the first NAL unit in the buffer
    stream_ptr = parse_buffer;
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        nalu_found = IS_NAL_UNIT_START(stream_ptr) ||
                    IS_NAL_UNIT_START1(stream_ptr);
        if (nalu_found)
        {
            break;
        }
        stream_ptr++;
    }

    // Reached end of buffer but could not find NAL unit
    if (!nalu_found)
    {
        cerr << "Could not read nal unit from file. File seems to be corrupted"
            << endl;
        return -1;
    }

    memcpy(buffer_ptr, stream_ptr, 4);
    buffer_ptr += 4;
    buffer->planes[0].bytesused = 4;
    stream_ptr += 4;

    // Copy bytes till the next NAL unit is found
    while ((stream_ptr - parse_buffer) < (bytes_read - 3))
    {
        if (IS_NAL_UNIT_START(stream_ptr) || IS_NAL_UNIT_START1(stream_ptr))
        {
            streamsize seekto = stream_initial_pos +
                    (stream_ptr - parse_buffer);
            if(stream->eof())
            {
                stream->clear();
            }
            stream->seekg(seekto, stream->beg);
            return 0;
        }
        *buffer_ptr = *stream_ptr;
        buffer_ptr++;
        stream_ptr++;
        buffer->planes[0].bytesused++;
    }

    // Reached end of buffer but could not find NAL unit
    cerr << "Could not read nal unit from file. File seems to be corrupted"
            << endl;
    return -1;
}

static int
read_decoder_input_chunk(ifstream * stream, NvBuffer * buffer)
{
    //length is the size of the buffer in bytes
    streamsize bytes_to_read = MIN(CHUNK_SIZE, buffer->planes[0].length);

    stream->read((char *) buffer->planes[0].data, bytes_to_read);
    // It is necessary to set bytesused properly, so that decoder knows how
    // many bytes in the buffer are valid
    buffer->planes[0].bytesused = stream->gcount();
    return 0;
}

static void
abort(context_t *ctx)
{
    ctx->got_error = true;
    ctx->dec->abort();
    if (ctx->conv)
    {
        ctx->conv->abort();
        pthread_cond_broadcast(&ctx->queue_cond);
    }
}

static void
get_rect(context_t *ctx)
{
    unsigned int i;
    string line;
    int frameNum = 0;
    int rect_id = 0;
    ctx->g_rect_num = 0;

    if (ctx->osd_file->is_open())
    {
        while (getline(*ctx->osd_file, line))
        {
            std::string frameNumPrefix = "frame:";
            std::string rectNumPrefix = " has rect:";
            std::string xywhPrefix = "x,y,w,h:";
            if (line.compare(0, frameNumPrefix.size(), frameNumPrefix) == 0)
            {
                string strFrameNum = line.substr(6, line.find(rectNumPrefix) - 6);
                frameNum = atoi(strFrameNum.c_str());
                string strRectNum =
                            line.substr(line.find(rectNumPrefix) + rectNumPrefix.size(),
                                        line.size());
                ctx->g_rect_num = atoi(strRectNum.c_str());

                if (log_level >= LOG_LEVEL_DEBUG)
                {
                    cout << "frameNum: " << frameNum
                         << " Rect Num: " << ctx->g_rect_num << endl;
                }
            }
            else if (std::string::npos != line.find(xywhPrefix))
            {
                string xywh = line.substr(line.find(xywhPrefix) + xywhPrefix.size(),
                                          line.size());
                for (i = 0; i < xywh.size(); ++i)
                {
                    if (isspace(xywh.at(i))) xywh.replace(i, 1, 1, ':');
                }
                string x = xywh.substr(0, xywh.find(":"));
                xywh = xywh.substr(x.size() + 1);
                string y = xywh.substr(0, xywh.find(":"));
                xywh = xywh.substr(y.size() + 1);
                string w = xywh.substr(0, xywh.find(":"));
                xywh = xywh.substr(w.size() + 1);
                string h = xywh.substr(0);
                ctx->g_rect[rect_id].left =  atof(x.c_str()) * ctx->dec_width;
                ctx->g_rect[rect_id].top =  atof(y.c_str()) * ctx->dec_height;
                ctx->g_rect[rect_id].width = atof(w.c_str()) * ctx->dec_width;
                ctx->g_rect[rect_id].height = atof(h.c_str()) * ctx->dec_height;

                if (log_level >= LOG_LEVEL_DEBUG)
                {
                    cout << "xywh: " << ctx->g_rect[rect_id].left << ":"
                                     << ctx->g_rect[rect_id].top << ":"
                                     << ctx->g_rect[rect_id].width << ":"
                                     << ctx->g_rect[rect_id].height << endl;
                }

                if (ctx->g_rect[rect_id].width < 10 ||
                    ctx->g_rect[rect_id].height < 10)
                {
                    if (log_level >= LOG_LEVEL_WARN)
                    {
                        cout << "Invalid xywh." << endl;
                    }
                    ctx->g_rect_num--;
                    continue;
                }

                for (i = 0; i < (unsigned int) ctx->g_rect_num; ++i)
                {
                    ctx->g_rect[i].border_width = BORDER_WIDTH;
                    ctx->g_rect[i].border_color.red = 1.0f;
                    ctx->g_rect[i].border_color.green = 0.0;
                    ctx->g_rect[i].border_color.blue = 0.0;
                }
                rect_id ++;
            }
            else
            {
                break;
            }
        }
    }
}

static bool
conv0_output_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                   NvBuffer * buffer, NvBuffer * shared_buffer,
                                   void *arg)
{
    context_t *ctx = (context_t *) arg;
    struct v4l2_buffer dec_capture_ret_buffer;
    struct v4l2_plane planes[MAX_PLANES];

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv0 output plane" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    memset(&dec_capture_ret_buffer, 0, sizeof(dec_capture_ret_buffer));
    memset(planes, 0, sizeof(planes));

    dec_capture_ret_buffer.index = shared_buffer->index;
    dec_capture_ret_buffer.m.planes = planes;

    pthread_mutex_lock(&ctx->queue_lock);
    ctx->conv_output_plane_buf_queue->push(buffer);

    // Return the buffer dequeued from converter output plane
    // back to decoder capture plane
    if (ctx->dec->capture_plane.qBuffer(dec_capture_ret_buffer, NULL) < 0)
    {
        abort(ctx);
        return false;
    }

    pthread_cond_broadcast(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_lock);

    return true;
}

static bool
conv0_capture_dqbuf_thread_callback(struct v4l2_buffer *v4l2_buf,
                                    NvBuffer * buffer, NvBuffer * shared_buffer,
                                    void *arg)
{
    context_t *ctx = (context_t *) arg;

    if (!v4l2_buf)
    {
        cerr << "Failed to dequeue buffer from conv0 output plane" << endl;
        abort(ctx);
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    // Create EGLImage from dmabuf fd
    ctx->egl_image = NvEGLImageFromFd(ctx->egl_display, buffer->planes[0].fd);
    if (ctx->egl_image == NULL)
    {
        fprintf(stderr, "Error while mapping dmabuf fd (0x%X) to EGLImage\n",
                 buffer->planes[0].fd);
        return false;
    }

    // Running algo process with EGLImage via GPU multi cores
    HandleEGLImage(&ctx->egl_image);

    // Destroy EGLImage
    NvDestroyEGLImage(ctx->egl_display, ctx->egl_image);
    ctx->egl_image = NULL;

    if (ctx->enable_osd) {
        get_rect(ctx);
    }

    if (ctx->g_rect_num > 0) {
        nvosd_draw_rectangles(ctx->nvosd_context,
                              MODE_HW,
                              buffer->planes[0].fd,
                              ctx->g_rect_num,
                              ctx->g_rect);
     }

    // Write raw video frame to file and return the buffer to converter
    // capture plane
    if (ctx->out_file)
    {
        write_video_frame(ctx->out_file, *buffer);
    }

    if (!ctx->disable_rendering)
    {
        ctx->renderer->render(buffer->planes[0].fd);
    }

    if (ctx->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
    {
        return false;
    }
    return true;
}

static int
sendEOStoConverter(context_t *ctx)
{
    // Check if converter is running
    if (ctx->conv->output_plane.getStreamStatus())
    {
        NvBuffer *conv_buffer;
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(&planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        pthread_mutex_lock(&ctx->queue_lock);
        while (ctx->conv_output_plane_buf_queue->empty())
        {
            pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
        }
        conv_buffer = ctx->conv_output_plane_buf_queue->front();
        ctx->conv_output_plane_buf_queue->pop();
        pthread_mutex_unlock(&ctx->queue_lock);

        v4l2_buf.index = conv_buffer->index;

        // Queue EOS buffer on converter output plane
        return ctx->conv->output_plane.qBuffer(v4l2_buf, NULL);
    }
    return 0;
}

static void
query_and_set_capture(context_t * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;
    uint32_t window_width;
    uint32_t window_height;

    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
               "Error: Could not get crop from decoder capture plane", error);

    cout << "Video Resolution: " << crop.c.width << "x" << crop.c.height
        << endl;

    if (ctx->enable_osd)
    {
        ctx->dec_width = crop.c.width;
        ctx->dec_height = crop.c.height;
    }

    // For file write, first deinitialize output and capture planes
    // of video converter and then use the new resolution from
    // decoder event resolution change
    if (ctx->conv)
    {
        ret = sendEOStoConverter(ctx);
        TEST_ERROR(ret < 0,
                   "Error while queueing EOS buffer on converter output",
                   error);

        // Wait for EOS buffer to arrive on capture plane
        ctx->conv->capture_plane.waitForDQThread(2000);

        ctx->conv->output_plane.deinitPlane();
        ctx->conv->capture_plane.deinitPlane();

        while(!ctx->conv_output_plane_buf_queue->empty())
        {
            ctx->conv_output_plane_buf_queue->pop();
        }
    }

    if (!ctx->disable_rendering)
    {
        // Destroy the old instance of renderer as resolution might have changed
        delete ctx->renderer;

        if (ctx->fullscreen)
        {
            // Required for fullscreen
            window_width = window_height = 0;
        }
        else if (ctx->window_width && ctx->window_height)
        {
            // As specified by user on commandline
            window_width = ctx->window_width;
            window_height = ctx->window_height;
        }
        else
        {
            // Resolution got from the decoder
            window_width = crop.c.width;
            window_height = crop.c.height;
        }

        // If height or width are set to zero, EglRenderer creates a fullscreen
        // window
        ctx->renderer =
            NvEglRenderer::createEglRenderer("renderer0", window_width,
                                           window_height, ctx->window_x,
                                           ctx->window_y);
        TEST_ERROR(!ctx->renderer,
                   "Error in setting up renderer. "
                   "Check if X is running or run with --disable-rendering",
                   error);

        ctx->renderer->setFPS(ctx->fps);
    }

    // deinitPlane unmaps the buffers and calls REQBUFS with count 0
    dec->capture_plane.deinitPlane();

    // Not necessary to call VIDIOC_S_FMT on decoder capture plane.
    // But decoder setCapturePlaneFormat function updates the class variables
    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat,
                                     format.fmt.pix_mp.width,
                                     format.fmt.pix_mp.height);
    TEST_ERROR(ret < 0, "Error in setting decoder capture plane format", error);

    // Get the minimum buffers which have to be requested on the capture plane
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    TEST_ERROR(ret < 0,
               "Error while getting value of minimum capture plane buffers",
               error);

    // Request (min + 5) buffers, export and map buffers
    ret =
        dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                       min_dec_capture_buffers + 5, false,
                                       false);
    TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);

    // For file write, first deinitialize output and capture planes
    // of video converter and then use the new resolution from
    // decoder event resolution change
    if (ctx->conv)
    {
        ret = ctx->conv->setOutputPlaneFormat(format.fmt.pix_mp.pixelformat,
                                              format.fmt.pix_mp.width,
                                              format.fmt.pix_mp.height,
                                              V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
        TEST_ERROR(ret < 0, "Error in converter output plane set format",
                   error);

        ret = ctx->conv->setCapturePlaneFormat((ctx->out_pixfmt == 1 ?
                                                    V4L2_PIX_FMT_NV12M :
                                                    V4L2_PIX_FMT_YUV420M),
                                                crop.c.width,
                                                crop.c.height,
                                                V4L2_NV_BUFFER_LAYOUT_PITCH);
        TEST_ERROR(ret < 0, "Error in converter capture plane set format",
                   error);

        ret = ctx->conv->setCropRect(0, 0, crop.c.width, crop.c.height);
        TEST_ERROR(ret < 0, "Error while setting crop rect", error);

        ret =
            ctx->conv->output_plane.setupPlane(V4L2_MEMORY_DMABUF,
                                                dec->capture_plane.
                                                getNumBuffers(), false, false);
        TEST_ERROR(ret < 0, "Error in converter output plane setup", error);

        ret =
            ctx->conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                                 dec->capture_plane.
                                                 getNumBuffers(), true, false);
        TEST_ERROR(ret < 0, "Error in converter capture plane setup", error);

        ret = ctx->conv->output_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamon", error);

        ret = ctx->conv->capture_plane.setStreamStatus(true);
        TEST_ERROR(ret < 0, "Error in converter output plane streamoff", error);

        // Add all empty conv output plane buffers to conv_output_plane_buf_queue
        for (uint32_t i = 0; i < ctx->conv->output_plane.getNumBuffers(); i++)
        {
            ctx->conv_output_plane_buf_queue->push(ctx->conv->output_plane.
                    getNthBuffer(i));
        }

        for (uint32_t i = 0; i < ctx->conv->capture_plane.getNumBuffers(); i++)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));

            v4l2_buf.index = i;
            v4l2_buf.m.planes = planes;
            ret = ctx->conv->capture_plane.qBuffer(v4l2_buf, NULL);
            TEST_ERROR(ret < 0, "Error Qing buffer at converter capture plane",
                       error);
        }
        ctx->conv->output_plane.startDQThread(ctx);
        ctx->conv->capture_plane.startDQThread(ctx);

    }

    // Capture plane STREAMON
    ret = dec->capture_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in decoder capture plane streamon", error);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        ret = dec->capture_plane.qBuffer(v4l2_buf, NULL);
        TEST_ERROR(ret < 0, "Error Qing buffer at output plane", error);
    }

    cout << "Query and set capture successful" << endl;
    return;

error:
    if (error)
    {
        abort(ctx);
        cerr << "Error in " << __func__ << endl;
    }
}

static void *
dec_capture_loop_fcn(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_event ev;
    int ret;

    cout << "Starting decoder capture loop thread" << endl;
    prctl (PR_SET_NAME, "dec_cap", 0, 0, 0);

    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do
    {
        ret = dec->dqEvent(ev, 1000);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cerr <<
                    "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE"
                    << endl;
            }
            else
            {
                cerr << "Error in dequeueing decoder event" << endl;
            }
            abort(ctx);
            break;
        }
    }
    while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE);

    // query_and_set_capture acts on the resolution change event
    if (!ctx->got_error)
        query_and_set_capture(ctx);

    // Exit on error or EOS which is signalled in main()
    while (!(ctx->got_error || dec->isInError() || ctx->got_eos))
    {
        NvBuffer *dec_buffer;

        // Check for Resolution change again
        ret = dec->dqEvent(ev, false);
        if (ret == 0)
        {
            switch (ev.type)
            {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    query_and_set_capture(ctx);
                    continue;
            }
        }

        while (1)
        {
            struct v4l2_buffer v4l2_buf;
            struct v4l2_plane planes[MAX_PLANES];

            memset(&v4l2_buf, 0, sizeof(v4l2_buf));
            memset(planes, 0, sizeof(planes));
            v4l2_buf.m.planes = planes;

            // Dequeue a filled buffer
            if (dec->capture_plane.dqBuffer(v4l2_buf, &dec_buffer, NULL, 0))
            {
                if (errno == EAGAIN)
                {
                    usleep(1000);
                }
                else
                {
                    abort(ctx);
                    cerr << "Error while calling dequeue at capture plane" <<
                        endl;
                }
                break;
            }

            // Give the buffer to video converter output plane
            if (ctx->conv)
            {
                NvBuffer *conv_buffer;
                struct v4l2_buffer conv_output_buffer;
                struct v4l2_plane conv_planes[MAX_PLANES];

                memset(&conv_output_buffer, 0, sizeof(conv_output_buffer));
                memset(conv_planes, 0, sizeof(conv_planes));
                conv_output_buffer.m.planes = conv_planes;

                // Get an empty conv output plane buffer from conv_output_plane_buf_queue
                pthread_mutex_lock(&ctx->queue_lock);
                while (ctx->conv_output_plane_buf_queue->empty())
                {
                    pthread_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
                }
                conv_buffer = ctx->conv_output_plane_buf_queue->front();
                ctx->conv_output_plane_buf_queue->pop();
                pthread_mutex_unlock(&ctx->queue_lock);

                conv_output_buffer.index = conv_buffer->index;

                if (ctx->conv->output_plane.
                        qBuffer(conv_output_buffer, dec_buffer) < 0)
                {
                    abort(ctx);
                    cerr <<
                        "Error while queueing buffer at converter output plane"
                        << endl;
                    break;
                }
            }
            else
            {
                if (ctx->dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
                {
                    abort(ctx);
                    cerr << "Error while queueing buffer at decoder capture plane"
                         << endl;
                    break;
                }
            }
        }
    }

    // Send EOS to converter
    if (ctx->conv)
    {
        if (sendEOStoConverter(ctx) < 0)
        {
            cerr << "Error while queueing EOS buffer on converter output"
                 << endl;
        }
    }

    cout << "Exiting decoder capture loop thread" << endl;
    return NULL;
}

static void
set_defaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->fullscreen = false;
    ctx->window_height = 0;
    ctx->window_width = 0;
    ctx->window_x = 0;
    ctx->window_y = 0;
    ctx->out_pixfmt = 1;
    ctx->fps = 30;
    ctx->nvosd_context = NULL;

    ctx->conv_output_plane_buf_queue = new queue < NvBuffer * >;
    pthread_mutex_init(&ctx->queue_lock, NULL);
    pthread_cond_init(&ctx->queue_cond, NULL);
}

int
main(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;
    int error = 0;
    uint32_t i;
    bool eos = false;
    char *nalu_parse_buffer = NULL;

    set_defaults(&ctx);

    if (parse_csv_args(&ctx, argc, argv))
    {
        fprintf(stderr, "Error parsing commandline arguments\n");
        return -1;
    }

    // Get defalut EGL display
    ctx.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx.egl_display == EGL_NO_DISPLAY)
    {
        fprintf(stderr, "Error while get EGL display connection\n");
        return -1;
    }

    // Init EGL display connection
    if (!eglInitialize(ctx.egl_display, NULL, NULL))
    {
        fprintf(stderr, "Erro while initialize EGL display connection\n");
        return -1;
    }

    if (ctx.enable_osd) {
        ctx.nvosd_context = nvosd_create_context();
        cout << "ctx.osd_file_path:" << ctx.osd_file_path << endl;

        ctx.osd_file = new ifstream(ctx.osd_file_path);
        TEST_ERROR(!ctx.osd_file->is_open(), "Error opening osd file", cleanup);
    }

    ctx.dec = NvVideoDecoder::createVideoDecoder("dec0");
    TEST_ERROR(!ctx.dec, "Could not create decoder", cleanup);

    // Subscribe to Resolution change event
    ret = ctx.dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE",
            cleanup);

    if (ctx.input_nalu)
    {
        nalu_parse_buffer = new char[CHUNK_SIZE];
    }
    else
    {
        // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
        // so that application can send chunks of encoded data instead of forming
        // complete frames. This needs to be done before setting format on the
        // output plane.
        ret = ctx.dec->disableCompleteFrameInputBuffer();
        TEST_ERROR(ret < 0,
                "Error in decoder disableCompleteFrameInputBuffer", cleanup);
    }

    // Set format on the output plane
    ret = ctx.dec->setOutputPlaneFormat(ctx.decoder_pixfmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    // Query, Export and Map the output plane buffers so that we can read
    // encoded data into the buffers
    ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Error opening input file", cleanup);

    if (ctx.out_file_path)
    {
        ctx.out_file = new ofstream(ctx.out_file_path);
        TEST_ERROR(!ctx.out_file->is_open(), "Error opening output file",
                cleanup);
    }

    if (ctx.out_file || !ctx.disable_rendering)
    {
        // Create converter to convert from BL to PL for writing raw video
        // to file or crop the frame and display
        ctx.conv = NvVideoConverter::createVideoConverter("conv0");
        TEST_ERROR(!ctx.conv, "Could not create video converter", cleanup);
        ctx.conv->output_plane.
            setDQThreadCallback(conv0_output_dqbuf_thread_callback);
        ctx.conv->capture_plane.
            setDQThreadCallback(conv0_capture_dqbuf_thread_callback);

    }

    ret = ctx.dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);

    pthread_create(&ctx.dec_capture_loop, NULL, dec_capture_loop_fcn, &ctx);

    // Read encoded data and enqueue all the output plane buffers.
    // Exit loop in case file read is complete.
    i = 0;
    while (!eos && !ctx.got_error && !ctx.dec->isInError() &&
            i < ctx.dec->output_plane.getNumBuffers())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = ctx.dec->output_plane.getNthBuffer(i);
        if (ctx.input_nalu)
        {
            read_decoder_input_nalu(ctx.in_file, buffer, nalu_parse_buffer,
                    CHUNK_SIZE);
        }
        else
        {
            read_decoder_input_chunk(ctx.in_file, buffer);
        }

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        // It is necessary to queue an empty buffer to signal EOS to the decoder
        // i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
        ret = ctx.dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
        i++;
    }

    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    while (!eos && !ctx.got_error && !ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        ret = ctx.dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }

        if (ctx.input_nalu)
        {
            read_decoder_input_nalu(ctx.in_file, buffer, nalu_parse_buffer,
                    CHUNK_SIZE);
        }
        else
        {
            read_decoder_input_chunk(ctx.in_file, buffer);
        }
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        ret = ctx.dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }
        if (v4l2_buf.m.planes[0].bytesused == 0)
        {
            eos = true;
            cout << "Input file read complete" << endl;
            break;
        }
    }

    // After sending EOS, all the buffers from output plane should be dequeued.
    // and after that capture plane loop should be signalled to stop.
    while (ctx.dec->output_plane.getNumQueuedBuffers() > 0 &&
           !ctx.got_error && !ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        ret = ctx.dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(&ctx);
            break;
        }
    }

    // Signal EOS to the decoder capture loop
    ctx.got_eos = true;

    if (ctx.conv)
    {
        ctx.conv->capture_plane.waitForDQThread(-1);
    }

cleanup:
    if (ctx.dec_capture_loop)
    {
        pthread_join(ctx.dec_capture_loop, NULL);
    }

    if (ctx.dec && ctx.dec->isInError())
    {
        cerr << "Decoder is in error" << endl;
        error = 1;
    }
    if (ctx.got_error)
    {
        error = 1;
    }

    // The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
    // unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
    // and finally call v4l2_close on the fd.
    delete ctx.dec;
    delete ctx.conv;

    // Similarly, EglRenderer destructor does all the cleanup
    delete ctx.renderer;
    delete ctx.in_file;
    delete ctx.out_file;
    delete ctx.conv_output_plane_buf_queue;
    delete []nalu_parse_buffer;

    free(ctx.in_file_path);
    free(ctx.out_file_path);
    if (ctx.enable_osd) {
        ctx.osd_file->close();
        free(ctx.osd_file_path);
    }

    // Terminate EGL display connection
    if (ctx.egl_display)
    {
        if(!eglTerminate(ctx.egl_display))
        {
            fprintf(stderr, "Error while terminate EGL display connection\n");
            return -1;
        }
    }
    if (ctx.enable_osd)
    {
        nvosd_destroy_context(ctx.nvosd_context);
        ctx.nvosd_context = NULL;
    }

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
