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

#include "video_dec_gie_main.h"

#include <errno.h>
#include <fstream>
#include <iostream>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <memory>
#include <chrono>

#include <cuda.h>
#include <cuda_runtime.h>

#include "NvUtils.h"
#include "NvCudaProc.h"
#include "nvbuf_utils.h"
#include "video_dec_gie.h"
#include "gie_inference.h"

#if defined(NX_TEST_MODE)
#include "out_rgb.h"
#endif

#include <nx/kit/debug.h>

#include "tegra_video_ini.h"
#include "video_dec_gie_ini.h"

#include "main_tv.h"

namespace {

class Lock
{
public:
    Lock(pthread_mutex_t* mutex): m_mutex(mutex) { pthread_mutex_lock(m_mutex); }
    ~Lock() { pthread_mutex_unlock(m_mutex); }

private:
    pthread_mutex_t* m_mutex;
};

} // namespace

#define USE_CPU_FOR_INTFLOAT_CONVERSION 0

#define TEST_ERROR(cond, str, label) if(cond) { \
                           cerr << str << endl; \
                           error = 1; \
                           goto label; }

#define CHUNK_SIZE 4000000

#define IS_NAL_UNIT_START(buffer_ptr) \
            (!buffer_ptr[0] && !buffer_ptr[1] && \
             !buffer_ptr[2] && (buffer_ptr[3] == 1))

using namespace std;
using namespace nvinfer1;
using namespace nvcaffeparser1;

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

static bool
initConv(context_t * ctx)
{
    int ret = 0;
    struct v4l2_format format;

    ret = ctx->dec->capture_plane.getFormat(format);
    if (ret < 0)
    {
        cerr << "Error: Could not get format from decoder capture plane" << endl;
        return false;
    }

    ret = ctx->conv->setOutputPlaneFormat(format.fmt.pix_mp.pixelformat,
                                          format.fmt.pix_mp.width,
                                          format.fmt.pix_mp.height,
                                          V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    if(ret < 0)
    {
        cerr << "Error in converter output plane set format" << endl;
        return false;
    }

    // 1. Convert YUV to RGB
    // 2. Scale to the resolution GIE needs
    // 3. Convert Block buffer layout to Pitch buffer layout
    ret = ctx->conv->setCapturePlaneFormat(V4L2_PIX_FMT_ABGR32,
                                           ctx->gie_ctx->getNetWidth(),
                                           ctx->gie_ctx->getNetHeight(),
                                           V4L2_NV_BUFFER_LAYOUT_PITCH);
    if (ret < 0)
    {
        cerr << "Error in converter capture plane set format" << endl;
        return false;
    }

    const int x = ini().cropRectX >= 0 ? ini().cropRectX : 0;
    const int y = ini().cropRectY >= 0 ? ini().cropRectY : 0;
    const int w = ini().cropRectW >= 0 ? ini().cropRectW : ctx->dec_width;
    const int h = ini().cropRectH >= 0 ? ini().cropRectH : ctx->dec_height;
    NX_PRINT << "setCropRect(" << x << ", " << y << ", " << w << ", " << h << ")";
    ret = ctx->conv->setCropRect(x, y, w, h);
    if (ret < 0)
    {
        cerr << "Error while setting crop rect" << endl;
        return false;
    }

    ret = ctx->conv->output_plane.setupPlane(V4L2_MEMORY_DMABUF,
                                   ctx->dec->capture_plane.getNumBuffers(),
                                   false,
                                   false);
    if (ret < 0)
    {
        cerr << "Error in converter output plane setup" << endl;
        return false;
    }

    ret = ctx->conv->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                             ctx->dec->capture_plane.getNumBuffers(),
                             true,
                             false);
    if (ret < 0)
    {
        cerr << "Error in converter capture plane setup" << endl;
        return false;
    }

    ret = ctx->conv->output_plane.setStreamStatus(true);
    if (ret < 0)
    {
        cerr << "Error in converter output plane streamon" << endl;
        return false;
    }

    ret = ctx->conv->capture_plane.setStreamStatus(true);
    if (ret < 0)
    {
        cerr << "Error in converter output plane streamoff" << endl;
        return false;
    }

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
        if (ret < 0)
        {
            cerr << "Error Qing buffer at converter capture plane" << endl;
            return false;
        }
    }
    ctx->conv->output_plane.startDQThread(ctx);
    ctx->conv->capture_plane.startDQThread(ctx);

    return true;
}

static bool
deinitConv(context_t * ctx)
{
    int ret;

    ret = sendEOStoConverter(ctx);
    if (ret < 0)
    {
        cerr << "Error while queueing EOS buffer on converter output" << endl;
        return false;
    }
    // Wait for EOS buffer to arrive on capture plane
    ctx->conv->capture_plane.waitForDQThread(2000);

    ctx->conv->output_plane.deinitPlane();
    ctx->conv->capture_plane.deinitPlane();

    while(!ctx->conv_output_plane_buf_queue->empty())
    {
        ctx->conv_output_plane_buf_queue->pop();
    }
    return true;
}

static void
resChange(context_t * ctx)
{
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_format format;
    struct v4l2_crop crop;
    int32_t min_dec_capture_buffers;
    int ret = 0;
    int error = 0;

    // Get capture plane format from the decoder. This may change after
    // an resolution change event
    ret = dec->capture_plane.getFormat(format);
    TEST_ERROR(ret < 0,
               "Error: Could not get format from decoder capture plane", error);

    // Get the display resolution from the decoder
    ret = dec->capture_plane.getCrop(crop);
    TEST_ERROR(ret < 0,
               "Error: Could not get crop from decoder capture plane", error);
    ctx->dec_width = crop.c.width;
    ctx->dec_height = crop.c.height;

    cout << "Video Resolution: " << ctx->dec_width << "x" << ctx->dec_height << endl;

    // First deinitialize output and capture planes
    // of video converter and then use the new resolution from
    // decoder event resolution change
    ret = deinitConv(ctx);

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
    ret = dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP,
                                       min_dec_capture_buffers,
                                       false,
                                       false);
    TEST_ERROR(ret < 0, "Error in decoder capture plane setup", error);

    if (ctx->conv)
    {
        bool result = initConv(ctx);
        TEST_ERROR(result == false, "initConv failed", error);
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

    cout << "Resolution change successful" << endl;
    return;

error:
    if (error)
    {
        abort(ctx);
        cerr << "Error in " << __func__ << endl;
    }
}

// Give the buffer to video converter output plane
static bool
decFillBufConv(context_t *ctx, NvBuffer *dec_buf)
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

    if (ctx->conv->output_plane.qBuffer(conv_output_buffer, dec_buf) < 0)
    {
        abort(ctx);
        cerr << "Error while queueing buffer at converter output plane"
             << endl;
        return false;
    }
    return true;
}

static void*
decCaptureLoop(void *arg)
{
    context_t *ctx = (context_t *) arg;
    NvVideoDecoder *dec = ctx->dec;
    struct v4l2_event ev;
    int ret;

    cout << "Starting decoder capture loop thread" << endl;
    prctl (PR_SET_NAME, "decCap", 0, 0, 0);

    // Need to wait for the first Resolution change event, so that
    // the decoder knows the stream resolution and can allocate appropriate
    // buffers when we call REQBUFS
    do
    {
        ret = dec->dqEvent(ev, -1);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                cerr << "Timed out waiting for first V4L2_EVENT_RESOLUTION_CHANGE"
                     << endl;
            }
            else
            {
                cerr << "Error in dequeuing decoder event" << endl;
            }
            abort(ctx);
            break;
        }
    }
    while (ev.type != V4L2_EVENT_RESOLUTION_CHANGE);

    // Handle resolution change event
    if (!ctx->got_error)
        resChange(ctx);

    // Exit on error or EOS which is signalled in main()
    for (;;)
    {
        if (ctx->got_error)
        {
            cout << "Exiting decoder capture loop thread: ctx->got_error" << endl;
            break;
        }
        if (dec->isInError())
        {
            cout << "Exiting decoder capture loop thread: dec->isInError()" << endl;
            break;
        }
        if (ctx->got_eos)
        {
            cout << "Exiting decoder capture loop thread: ctx->got_eos" << endl;
            break;
        }

        NvBuffer *dec_buffer;

        // Check for Resolution change again
        ret = dec->dqEvent(ev, false);
        if (ret == 0)
        {
            switch (ev.type)
            {
                case V4L2_EVENT_RESOLUTION_CHANGE:
                    resChange(ctx);
                    continue;
            }
        }

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
                cerr << "Error while calling dequeue at capture plane"
                     << endl;
            }
            continue;
        }

        // Give the buffer to video converter output plane
        if (ctx->conv)
        {
            if (!decFillBufConv(ctx, dec_buffer))
                continue;
        }
    }

    cout << "Exiting decoder capture loop thread" << endl;
    return NULL;
}

#if defined(NX_TEST_MODE)
void rgbToRgba(unsigned char* rgb, int rgbSize, int* rgba)
{
    auto i = 0;
    auto j  = 0;

    for (i = 0; i < rgbSize; i += 3)
    {
        rgba[i / 3] = (rgb[i] << 24) + (rgb[i + 1] << 16) + (rgb[i + 2] << 8);
    }
}
#endif

static void*
gieThread(void *arg)
{
    using namespace std::chrono;

#if defined(NX_TEST_MODE)
    context_t *ctx = (context_t *)arg;
    while (true)
    {
        queue<vector<cv::Rect>> rectList_queue;
        std::cout << "=============!!!!!!!!!!!!!!!!!!!=================" << std::endl;
        void *cuda_buf = ctx->gie_ctx->getBuffer(0);
        std::unique_ptr<int[]> rgba(new int[512 *1024]);
        std::unique_ptr<float[]> rgb2(new float[512 *1024 *3]);

        rgbToRgba(MagickImage, 1024 * 512 * 3, rgba.get());

        for (auto i = 0; i < 20; i++)
        {
            int n = rgba.get()[i];
            std::cout << "RGBA: "
                << ((int)((n >> 24) & 0xFF)) << " "
                << ((int)((n >> 16) & 0xFF)) << " "
                << ((int)((n >> 8) & 0xFF)) << " "
                << ((int)(n & 0xFF)) << std::endl;
        }

        mapIntRgbaToFloatBgr(rgba.get(), 512 * 1024, cuda_buf);

        cudaMemcpy(rgb2.get(), cuda_buf, 512 * 1024 *3, cudaMemcpyDeviceToHost);
        cudaDeviceSynchronize();

        for (auto i = 0; i < 100; i++)
        {
            std::cout << "Component " << i << " " << rgb2[i] << std::endl;
        }

        ctx->gie_ctx->doInference(rectList_queue);

        auto vec = rectList_queue.front();
        rectList_queue.pop_front();

        for (const auto& rec: vec)
        {
            std::cout << "RECTANGLE: "
                << rec.x << " "
                << rec.y << " "
                << rec.width << " "
                << rec.height << std::endl;
        }
        sleep(1);
    }
#else

    std::cout << "GIE THREAD STARTED!!!" << std::endl;
    int buf_num = 0;
    EGLImageKHR egl_image = NULL;
    struct v4l2_buffer *v4l2_buf;
    NvBuffer *buffer;
    NvBuffer *shared_buffer;
    context_t *ctx = (context_t *)arg;
    int process_last_batch = 0;
    float *gie_inputbuf = ctx->gie_ctx->getInputBuf();
    prctl (PR_SET_NAME, "gieThread", 0, 0, 0);

    // Get default EGL display
    ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx->egl_display == EGL_NO_DISPLAY)
    {
        cerr << "Error while get EGL display connection" << endl;
        return NULL;
    }

    // Init EGL display connection
    if (!eglInitialize(ctx->egl_display, NULL, NULL))
    {
        cerr << "Erro while initialize EGL display connection" << endl;
        return NULL;
    }

    double inferenceFps = ini().maxInferenceFps;
    std::chrono::milliseconds minTimeToNextInference(0);
    if (inferenceFps != -1)
    {
        inferenceFps /= 10;
        minTimeToNextInference = std::chrono::milliseconds(
            (int)(1000.0 / inferenceFps));
    }

    while (1)
    {
        // wait for buffer for process to come
        Shared_Buffer gie_buffer;
        pthread_mutex_lock(&ctx->gie_lock);
        if(ctx->gie_buf_queue->empty())
        {
            if (ctx->gie_stop && buf_num == 0)
            {
                pthread_mutex_unlock(&ctx->gie_lock);
                break;
            }
            else if (!ctx->gie_stop)
            {
                // waits 50 ms for the frame to come
                struct timespec timeout;
                long time_in_ms = 50;
                timeout.tv_sec = time(0);
                timeout.tv_nsec = time_in_ms * 1000000;
                int ret =
                    pthread_cond_wait(&ctx->gie_cond, &ctx->gie_lock);
                if (ret != 0)
                {
                    pthread_mutex_unlock(&ctx->gie_lock);
                    continue;
                }
            }
        }

        if (ctx->gie_buf_queue->size() != 0)
        {
            gie_buffer = ctx->gie_buf_queue->front();
            ctx->gie_buf_queue->pop();
        }
        else
        {
            process_last_batch = 1;
        }


        if (process_last_batch == 0)
        {
#if 1
            const bool needToDropFrame = !ctx->m_ptsQueue.empty()
                && ctx->m_lastInferenceDuration + ctx->m_lastProcessedFrameTimestamp > microseconds(ctx->m_ptsQueue.front())
                && ctx->m_lastInferenceDuration != microseconds::zero();


            if (needToDropFrame)
            {
                NX_OUTPUT << "@@@@@@@@@@@@@@@@@@@@@@@ DROPPING frame with timestamp: " << ctx->m_ptsQueue.front()
                    << ", difference: "
                    << (ctx->m_lastInferenceDuration + ctx->m_lastProcessedFrameTimestamp - microseconds(ctx->m_ptsQueue.front())).count()
                    << ", " << (ctx->m_lastInferenceDuration + ctx->m_lastProcessedFrameTimestamp).count() 
                    << ", " << microseconds(ctx->m_ptsQueue.front()).count()
                    << std::endl;
                
                if (!ctx->m_ptsQueue.empty())
                    ctx->m_ptsQueue.pop();
                else
                    NX_OUTPUT << "####### Some strange error: there is a frame, but pts queue is empty";


                pthread_mutex_unlock(&ctx->gie_lock);

                // It's a hack, actually I don't know how it works at all. If this lines are not here
                // then we hang in pushCompressedFrame.
                if (ctx->conv->capture_plane.qBuffer(gie_buffer.v4l2_buf, NULL) < 0)
                    NX_OUTPUT << "conv queue buffer error";
                continue;
            }
#endif
        }

        pthread_mutex_unlock(&ctx->gie_lock);

        // we still have buffer, so accumulate buffer into batch
        if (process_last_batch == 0)
        {
            v4l2_buf           = &gie_buffer.v4l2_buf;
            buffer             = gie_buffer.buffer;
            shared_buffer      = gie_buffer.shared_buffer;
            ctx                = (context_t *)gie_buffer.arg;

            int batch_offset = buf_num  * ctx->gie_ctx->getNetWidth() *
                        ctx->gie_ctx->getNetHeight() * ctx->gie_ctx->getChannel();



            // map fd into EGLImage, then copy it with GPU in parallel
            // Create EGLImage from dmabuf fd
            egl_image = NvEGLImageFromFd(ctx->egl_display, buffer->planes[0].fd);
            if (egl_image == NULL)
            {
                cerr << "Error while mapping dmabuf fd ("
                     << buffer->planes[0].fd << ") to EGLImage"
                     << endl;

                return NULL;
            }

            void *cuda_buf = ctx->gie_ctx->getBuffer(0);

            // TODO: #mshevchenko: Here they convert RGB to float directly to NN buffer in VRAM.

            // map eglimage into GPU address
            mapEGLImage2Float2(
                &egl_image,
                ctx->gie_ctx->getNetWidth(),
                ctx->gie_ctx->getNetHeight(),
                (char *)cuda_buf + batch_offset * sizeof(float));

            // Destroy EGLImage
            NvDestroyEGLImage(ctx->egl_display, egl_image);
            egl_image = NULL;

            buf_num++;

            // now we push it to capture plane to let v4l2 go on
            if (ctx->conv->capture_plane.qBuffer(*v4l2_buf, NULL) < 0)
            {
                cout<<"conv queue buffer error"<<endl;
            }

            // buffer is not enough for a batch, continue to wait for buffer
            if (buf_num < ctx->gie_ctx->getBatchSize() && !ctx->gie_stop)
            {
                continue;
            }
        }
        else if(buf_num == 0) // framenum equal batch_size * n
        {
            pthread_mutex_unlock(&ctx->gie_lock);
            break;
        }

        // buffer comes, we begin to inference
        buf_num = 0;
        queue<vector<cv::Rect>> rectList_queue;

        NX_TIME_BEGIN(Inference);

        const auto before = high_resolution_clock::now();
        ctx->gie_ctx->doInference(rectList_queue);
        ctx->m_lastInferenceDuration = duration_cast<microseconds>(high_resolution_clock::now() - before);
        ctx->m_lastProcessedFrameTimestamp = microseconds(ctx->m_ptsQueue.front());

        NX_TIME_END(Inference);

        {
            pthread_mutex_lock(&ctx->gie_lock);
            while(!rectList_queue.empty())
            {
                vector<cv::Rect> rectList = rectList_queue.front();
                rectList_queue.pop();

                const auto pts = ctx->m_ptsQueue.front();
                ctx->m_ptsQueue.pop();             
                ctx->m_outPtsQueue.push(pts);

                ctx->rectQueuePtr->push(rectList);
                for (int i = 0; i < rectList.size(); i++)
                {
                    cv::Rect &r = rectList[i];
                    std::cout <<"    x " << r.x << ", y " << r.y
                         << ", width " << r.width << ", height " << r.height
                         << endl;
                }
            }

            ++ctx->m_framesProcessed;
            auto now = std::chrono::high_resolution_clock::now();
            auto totalTimeElapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                (now - ctx->m_firstFrameArrivalTime)).count();

            if (ctx->m_framesProcessed)
            {
                NX_OUTPUT << "####### FRAME RATE IS: "
                    <<  ctx->m_framesProcessed / (double) totalTimeElapsedSeconds;
            }

            pthread_mutex_unlock(&ctx->gie_lock);
        }

        if (process_last_batch == 1)
        {
            break;
        }
    }

    // Terminate EGL display connection
    if (ctx->egl_display)
    {
        if (!eglTerminate(ctx->egl_display))
        {
            cerr << "Error while terminate EGL display connection\n" << endl;
            return NULL;
        }
    }
#endif
    return NULL;
}

static bool
conv0_outputDqbufThreadCallback(struct v4l2_buffer *v4l2_buf,
                                       NvBuffer * buffer,
                                       NvBuffer * shared_buffer,
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
conv0_captureDqbufThreadCallback(struct v4l2_buffer *v4l2_buf,
                                    NvBuffer *buffer,
                                    NvBuffer *shared_buffer,
                                    void *arg)
{
    context_t * ctx = (context_t*) arg;

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

    //push buffer to process queue
    Shared_Buffer gie_buffer;

    // v4l2_buf is local in the DQthread and exists in the scope of the callback
    // function only and not in the entire application. The application has to
    // copy this for using at out of the callback.
    memcpy(&gie_buffer.v4l2_buf, v4l2_buf, sizeof(v4l2_buffer));

    // TODO: #mshevchenko: Here they got decoded frame: NvBuffer* buffer.

    gie_buffer.buffer = buffer;
    gie_buffer.shared_buffer = shared_buffer;
    gie_buffer.arg = arg;
    pthread_mutex_lock(&ctx->gie_lock);
    ctx->gie_buf_queue->push(gie_buffer);
    pthread_cond_broadcast(&ctx->gie_cond);
    pthread_mutex_unlock(&ctx->gie_lock);

    return true;
}

static void
setDefaults(context_t * ctx)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->deployfile = exeIni().deployFile;
    ctx->modelfile = exeIni().modelFile;
    ctx->cachefile = exeIni().cacheFile;
    ctx->gie_ctx = new GIE_Context;
    ctx->gie_ctx->setDumpResult(true);
    ctx->gie_buf_queue = new queue<Shared_Buffer>;
    ctx->needToStop = false;

    ctx->conv_output_plane_buf_queue = new queue < NvBuffer * >;
    pthread_mutex_init(&ctx->queue_lock, NULL);
    pthread_cond_init(&ctx->queue_cond, NULL);
}

static void
setDefaultsNx(
    context_t * ctx,
    std::string modelFileName,
    std::string deployFileName,
    std::string cacheFileName)
{
    memset(ctx, 0, sizeof(context_t));
    ctx->deployfile = deployFileName;
    ctx->modelfile = modelFileName;
    ctx->cachefile = cacheFileName;
    ctx->gie_ctx = new GIE_Context;
    ctx->gie_ctx->setDumpResult(true);
    ctx->gie_buf_queue = new queue<Shared_Buffer>;
    ctx->needToStop = false;

    ctx->conv_output_plane_buf_queue = new queue < NvBuffer * >;
    pthread_mutex_init(&ctx->queue_lock, NULL);
    pthread_cond_init(&ctx->queue_cond, NULL);
}

int
mainOriginal(int argc, char *argv[])
{
    context_t ctx;
    int ret = 0;
    int error = 0;
    uint32_t i;
    bool eos = false;
    char *nalu_parse_buffer = NULL;

    setDefaults(&ctx);

    if (parseCsvArgs(&ctx, ctx.gie_ctx, argc, argv))
    {
        cerr << "Error parsing commandline arguments." << endl;
        return -1;
    }

    ctx.in_file = new ifstream(ctx.in_file_path);
    TEST_ERROR(!ctx.in_file->is_open(), "Error opening input file", cleanup);

    // Step-1: Create Decoder
    ctx.dec = NvVideoDecoder::createVideoDecoder("dec0");
    TEST_ERROR(!ctx.dec, "Could not create decoder", cleanup);

    // Subscribe to Resolution change event
    ret = ctx.dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE", cleanup);

    // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
    // so that application can send chunks of encoded data instead of forming
    // complete frames. This needs to be done before setting format on the
    // output plane.
    ret = ctx.dec->disableCompleteFrameInputBuffer();
    TEST_ERROR(ret < 0, "Error in decoder disableCompleteFrameInputBuffer", cleanup);

    // Set format on the output plane
    ret = ctx.dec->setOutputPlaneFormat(ctx.decoder_pixfmt, 2 * 1024 * 1024);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    // V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
    // set format
    if (ctx.disable_dpb)
    {
        ret = ctx.dec->disableDPB();
        TEST_ERROR(ret < 0, "Error in disableDPB", cleanup);
    }

    // Query, Export and Map the output plane buffers so that we can read
    // encoded data into the buffers
    ret = ctx.dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

    // Step-2: Create Conv0
    // Create converter to convert from BL to PL for writing raw video
    // to file or crop the frame and display
    ctx.conv = NvVideoConverter::createVideoConverter("conv0");
    TEST_ERROR(!ctx.conv, "Could not create video converter", cleanup);
    ctx.conv->output_plane.setDQThreadCallback(conv0_outputDqbufThreadCallback);
    ctx.conv->capture_plane.setDQThreadCallback(conv0_captureDqbufThreadCallback);

    ctx.rectQueuePtr = new std::queue<std::vector<cv::Rect>>;

    // Step-3: Create GIE
    ctx.gie_ctx->buildGieContext(ctx.deployfile, ctx.modelfile, ctx.cachefile);
    pthread_create(&ctx.gie_thread_handle, NULL, gieThread, &ctx);

    // Start decoder after GIE
    ret = ctx.dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);
    pthread_create(&ctx.dec_capture_loop, NULL, decCaptureLoop, &ctx);

    // Step-4: Input encoded data to decoder until EOF.
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
        read_decoder_input_chunk(ctx.in_file, buffer);

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

        read_decoder_input_chunk(ctx.in_file, buffer);

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

    // Step-5: Handling EOS.
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

cleanup:
    // Signal EOS to the decoder capture loop
    ctx.got_eos = true;
    if (ctx.dec_capture_loop)
    {
        pthread_join(ctx.dec_capture_loop, NULL);
    }

    if (ctx.gie_stop == 0)
    {
        ctx.gie_stop = 1;
        pthread_cond_broadcast(&ctx.gie_cond);
        pthread_join(ctx.gie_thread_handle, NULL);
    }

    // Send EOS to converter
    if (ctx.conv)
    {
        if (sendEOStoConverter(&ctx) < 0)
        {
            cerr << "Error while queueing EOS buffer on converter output"
                 << endl;
        }
        ctx.conv->capture_plane.waitForDQThread(-1);
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
    //delete ctx.renderer;
    delete ctx.in_file;
    delete ctx.conv_output_plane_buf_queue;
    delete []nalu_parse_buffer;

    free(ctx.in_file_path);

    delete ctx.gie_buf_queue;
    ctx.gie_ctx->destroyGieContext();
    delete ctx.gie_ctx;

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

int
mainNx(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "ERROR: File not specified." << std::endl;
        return 1;
    }
    const char *const filename = argv[1];

    Detector detector(/*id*/ "0");
    const int result = detector.startInference(
        exeIni().modelFile, exeIni().deployFile, exeIni().cacheFile);
    if (result != 0)
        return result;

    std::cout << "Inference has been started" << std::endl;
    ifstream in_file;
    in_file.open(filename, ios::in | ios::binary);
    if (!in_file.is_open())
    {
        std::cerr << "Can't open input file " << filename << std::endl;
        return 1;
    }

#if !defined(NX_TEST_MODE)
    auto buf = new uint8_t[CHUNK_SIZE];
    for (;;) //< TODO: Decide how to interrupt the loop.
    {
        in_file.read((char*) buf, CHUNK_SIZE);
        auto dataSize = in_file.gcount();
        detector.pushCompressedFrame(buf, dataSize, 0);
        if (dataSize <= 0)
            break;
    }

    delete[] buf;
#endif
    return 0;
}

int
main(int argc, char *argv[])
{
    if (argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
    {
        std::cout << "Usage: " << argv[0] << " <mode> <file> ..." << std::endl;
        std::cout << "Here <mode> is one of: \"--original\", \"--nx\", \"--tv\"." << std::endl;
        return 1;
    }
    else if (strcmp(argv[1], "--original") == 0)
    {
        std::cout << "Running in --original mode." << std::endl;
        return mainOriginal(argc - 1, &argv[1]);
    }
    else if (strcmp(argv[1], "--nx") == 0)
    {
        std::cout << "Running in --nx mode." << std::endl;
        return mainNx(argc - 1, &argv[1]);
    }
    else if (strcmp(argv[1], "--tv") == 0)
    {
        std::cout << "Running in --tv mode." << std::endl;
        return mainTv(argc - 1, &argv[1]);
    }
    else
    {
        std::cout << "ERROR: Invalid <mode>. Run with -h for help." << std::endl;
        return 1;
    }
}

//-------------------------------------------------------------------------------------------------
// Detector - implementation is based on the code above.

int Detector::startInference(
    const std::string& modelFileName,
    const std::string& deployFileName,
    const std::string& cacheFileName)
{
    int ret = 0;
    int error = 0;
    setDefaultsNx(&m_ctx, modelFileName, deployFileName, cacheFileName);

    m_ctx.m_ptsQueue = std::queue<int64_t>();
    m_ctx.m_outPtsQueue = std::queue<int64_t>();

    m_ctx.decoder_pixfmt = V4L2_PIX_FMT_H264;

    // Step-1: Create Decoder
    m_ctx.dec = NvVideoDecoder::createVideoDecoder(m_videoDecoderName.c_str());
    TEST_ERROR(!m_ctx.dec, "Could not create decoder", cleanup);

    // Subscribe to Resolution change event
    ret = m_ctx.dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, "Could not subscribe to V4L2_EVENT_RESOLUTION_CHANGE", cleanup);

    // Set V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT control to false
    // so that application can send chunks of encoded data instead of forming
    // complete frames. This needs to be done before setting format on the
    // output plane.
    ret = m_ctx.dec->disableCompleteFrameInputBuffer();
    TEST_ERROR(ret < 0, "Error in decoder disableCompleteFrameInputBuffer", cleanup);

    // Set format on the output plane
    ret = m_ctx.dec->setOutputPlaneFormat(m_ctx.decoder_pixfmt, 2 * 1024 * 1024);
    TEST_ERROR(ret < 0, "Could not set output plane format", cleanup);

    // V4L2_CID_MPEG_VIDEO_DISABLE_DPB should be set after output plane
    // set format
    if (m_ctx.disable_dpb)
    {
        ret = m_ctx.dec->disableDPB();
        TEST_ERROR(ret < 0, "Error in disableDPB", cleanup);
    }

    // Query, Export and Map the output plane buffers so that we can read
    // encoded data into the buffers
    ret = m_ctx.dec->output_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    TEST_ERROR(ret < 0, "Error while setting up output plane", cleanup);

    // Step-2: Create Conv0
    // Create converter to convert from BL to PL for writing raw video
    // to file or crop the frame and display
    m_ctx.conv = NvVideoConverter::createVideoConverter(m_videoConverterName.c_str());
    TEST_ERROR(!m_ctx.conv, "Could not create video converter", cleanup);
    m_ctx.conv->output_plane.setDQThreadCallback(conv0_outputDqbufThreadCallback);
    m_ctx.conv->capture_plane.setDQThreadCallback(conv0_captureDqbufThreadCallback);

    m_ctx.rectQueuePtr = &m_rects;

    std::cout << "Creating GIE thread" << std::endl;
    // Step-3: Create GIE
    m_ctx.gie_ctx->buildGieContext(m_ctx.deployfile, m_ctx.modelfile, m_ctx.cachefile);
    std::cout << "GIE context has been built" << std::endl;
    pthread_create(&m_ctx.gie_thread_handle, NULL, gieThread, &m_ctx);

#if !defined(NX_TEST_MODE)
    std::cout << "Creating decoder thread" << std::endl;
    // Start decoder after GIE
    ret = m_ctx.dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, "Error in output plane stream on", cleanup);
    pthread_create(&m_ctx.dec_capture_loop, NULL, decCaptureLoop, &m_ctx);
#endif

cleanup:
    return -error;

#if 0

    // Step-5: Handling EOS.
    // After sending EOS, all the buffers from output plane should be dequeued.
    // and after that capture plane loop should be signalled to stop.
    while (m_ctx.dec->output_plane.getNumQueuedBuffers() > 0 &&
           !m_ctx.got_error && !m_ctx.dec->isInError())
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;
        ret = m_ctx.dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(&m_ctx);
            break;
        }
    }

cleanup:
    // Signal EOS to the decoder capture loop
    m_ctx.got_eos = true;
    if (m_ctx.dec_capture_loop)
    {
        pthread_join(m_ctx.dec_capture_loop, NULL);
    }

    if (m_ctx.gie_stop == 0)
    {
        m_ctx.gie_stop = 1;
        pthread_cond_broadcast(&m_ctx.gie_cond);
        pthread_join(m_ctx.gie_thread_handle, NULL);
    }

    // Send EOS to converter
    if (m_ctx.conv)
    {
        if (sendEOStoConverter(&m_ctx) < 0)
        {
            cerr << "Error while queueing EOS buffer on converter output"
                 << endl;
        }
        m_ctx.conv->capture_plane.waitForDQThread(-1);
    }

    if (m_ctx.dec && m_ctx.dec->isInError())
    {
        cerr << "Decoder is in error" << endl;
        error = 1;
    }
    if (m_ctx.got_error)
    {
        error = 1;
    }

    // The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
    // unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
    // and finally call v4l2_close on the fd.
    delete m_ctx.dec;
    delete m_ctx.conv;

    // Similarly, EglRenderer destructor does all the cleanup
    //delete ctx.renderer;
    delete m_ctx.in_file;
    delete m_ctx.conv_output_plane_buf_queue;
    delete []nalu_parse_buffer;

    free(m_ctx.in_file_path);

    delete m_ctx.gie_buf_queue;
    m_ctx.gie_ctx->destroyGieContext();
    delete m_ctx.gie_ctx;

    if (error)
    {
        cout << "App run failed" << endl;
    }
    else
    {
        cout << "App run was successful" << endl;
    }
    return -error;

#endif // 0
}

bool Detector::stopSync()
{
    NX_OUTPUT << "Detector::stopSync() BEGIN";

    bool error = false;
    m_ctx.got_eos = true;
    if (m_ctx.dec_capture_loop)
    {
        NX_OUTPUT << "Detector::stopSync(): Joining thread dec_capture_loop...";
        pthread_join(m_ctx.dec_capture_loop, NULL);
        NX_OUTPUT << "Detector::stopSync(): Joined thread dec_capture_loop";
    }

    if (m_ctx.gie_stop == 0)
    {
        NX_OUTPUT << "Detector::stopSync(): Joining thread gie_thread_handle...";
        m_ctx.gie_stop = 1;
        pthread_cond_broadcast(&m_ctx.gie_cond);
        pthread_join(m_ctx.gie_thread_handle, NULL);
        NX_OUTPUT << "Detector::stopSync(): Joined thread gie_thread_handle";
    }

    // Send EOS to converter
    if (m_ctx.conv)
    {
        if (sendEOStoConverter(&m_ctx) < 0)
            NX_PRINT << "Error while queueing EOS buffer on converter output";
        m_ctx.conv->capture_plane.waitForDQThread(-1);
    }

    if (m_ctx.dec && m_ctx.dec->isInError())
    {
        NX_PRINT << "Decoder is in error state";
        error = true;
    }
    if (m_ctx.got_error)
        error = true;

    // The decoder destructor does all the cleanup i.e set streamoff on output and capture planes,
    // unmap buffers, tell decoder to deallocate buffer (reqbufs ioctl with counnt = 0),
    // and finally call v4l2_close on the fd.
    delete m_ctx.dec;
    delete m_ctx.conv;

    // Similarly, EglRenderer destructor does all the cleanup
    //delete ctx.renderer;
    delete m_ctx.in_file;
    delete m_ctx.conv_output_plane_buf_queue;

    if (m_ctx.in_file_path)
        free(m_ctx.in_file_path);

    delete m_ctx.gie_buf_queue;
    NX_OUTPUT << "Detector::stopSync(): destroyGieContext() BEGIN";
    m_ctx.gie_ctx->destroyGieContext();
    NX_OUTPUT << "Detector::stopSync(): destroyGieContext() END";
    delete m_ctx.gie_ctx;

    if (error)
        NX_PRINT << "App run failed";
    else
        NX_PRINT << "App run was successful";

    NX_OUTPUT << "Detector::stopSync() END -> " << error;
    return error;
}

bool Detector::hasRectangles()
{
    Lock lock(&(m_ctx.gie_lock));
    return !m_rects.empty();
}

bool Detector::pushCompressedFrame(const uint8_t* data, int dataSize, int64_t ptsUs)
{
    // Step-4: Input encoded data to decoder until EOF.
    // Read encoded data and enqueue all the output plane buffers.
    // Exit loop in case file read is complete.

    if (dataSize <= 0)
        return false;

    bool isError = m_ctx.got_error || m_ctx.dec->isInError();
    auto numBuffers = m_ctx.dec->output_plane.getNumBuffers();

    {
        Lock lock(&m_ctx.gie_lock);
        if (m_ctx.m_firstFrameArrivalTime == std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(0)))
            m_ctx.m_firstFrameArrivalTime = std::chrono::high_resolution_clock::now();

        m_ctx.m_ptsQueue.push(ptsUs);
    }

    if (m_firstTimePush && !isError && m_firstTimePushBufferNum < numBuffers)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        buffer = m_ctx.dec->output_plane.getNthBuffer(m_firstTimePushBufferNum);
        fillBuffer(data, dataSize, buffer);

        v4l2_buf.index = m_firstTimePushBufferNum;
        v4l2_buf.m.planes = planes;
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        // It is necessary to queue an empty buffer to signal EOS to the decoder
        // i.e. set v4l2_buf.m.planes[0].bytesused = 0 and queue the buffer
        auto ret = m_ctx.dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(&m_ctx);
            return false;
        }

        m_firstTimePushBufferNum++;

        if (m_firstTimePushBufferNum >= numBuffers)
            m_firstTimePush = false;

        return true;
    }

    // Since all the output plane buffers have been queued, we first need to
    // dequeue a buffer from output plane before we can read new data into it
    // and queue it again.
    if (!isError)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer *buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.m.planes = planes;

        auto ret = m_ctx.dec->output_plane.dqBuffer(v4l2_buf, &buffer, NULL, -1);
        if (ret < 0)
        {
            cerr << "Error DQing buffer at output plane" << endl;
            abort(&m_ctx);
            return false;
        }

        fillBuffer(data, dataSize, buffer);
        v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;

        ret = m_ctx.dec->output_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0)
        {
            cerr << "Error Qing buffer at output plane" << endl;
            abort(&m_ctx);
            return false;
        }

    }

    return true;
}

std::vector<cv::Rect> Detector::getRectangles(int64_t* outPts)
{
    Lock lock(&(m_ctx.gie_lock));
    std::vector<cv::Rect> rects;

    if (!m_rects.empty())
    {
        rects = m_rects.front();
        m_rects.pop();
    }

    if (!m_ctx.m_outPtsQueue.empty())
    {
        NX_OUTPUT << "POPPING FROM m_outPtsQueue " << m_ctx.m_outPtsQueue.size();
        if (outPts)
            *outPts = m_ctx.m_outPtsQueue.front();
        m_ctx.m_outPtsQueue.pop();
    }

    return rects;
}

int Detector::fillBuffer(const uint8_t* data, int dataSize, NvBuffer* buffer)
{
    if (!buffer)
    {
        NX_PRINT << "ERROR: Detector::fillBuffer(dataSize: " << dataSize << ", buffer: null)";
        return 0;
    }

    streamsize bytes_to_read = MIN(dataSize, buffer->planes[0].length);
    memcpy(buffer->planes[0].data, data, bytes_to_read);
    buffer->planes[0].bytesused = (uint32_t) bytes_to_read;

    return (uint32_t) bytes_to_read;
}
