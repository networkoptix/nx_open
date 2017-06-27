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

#include "Error.h"
#include "Thread.h"

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>

#include <NvVideoEncoder.h>
#include <NvApplicationProfiler.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

using namespace Argus;
using namespace EGLStream;

// Constant configuration.
static const int    MAX_ENCODER_FRAMES = 5;
static const int    DEFAULT_FPS        = 30;

// Configurations which can be overrided by cmdline
static int          CAPTURE_TIME = 1; // In seconds.
static Size         STREAM_SIZE (640, 480);
static std::string  OUTPUT_FILENAME ("output.h264");
static uint32_t     ENCODER_PIXFMT = V4L2_PIX_FMT_H264;
static bool         DO_STAT = false;
static bool         VERBOSE_ENABLE = false;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)
#define CHECK_ERROR(expr) \
    do { \
        if ((expr) < 0) { \
            abort(); \
            ORIGINATE_ERROR(#expr " failed"); \
        } \
    } while (0);

namespace ArgusSamples
{

/*******************************************************************************
 * FrameConsumer thread:
 *   Creates an EGLStream::FrameConsumer object to read frames from the stream
 *   and create NvBuffers (dmabufs) from acquired frames before providing the
 *   buffers to V4L2 for video encoding. The encoder will save the encoded
 *   stream to disk.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(OutputStream* stream);
    ~ConsumerThread();

    bool isInError()
    {
        return m_gotError;
    }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    bool createVideoEncoder();
    void abort();

    static bool encoderCapturePlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer,
            void *arg);

    OutputStream* m_stream;
    UniqueObj<FrameConsumer> m_consumer;
    NvVideoEncoder *m_VideoEncoder;
    std::ofstream *m_outputFile;
    bool m_gotError;
};

ConsumerThread::ConsumerThread(OutputStream* stream) :
        m_stream(stream),
        m_VideoEncoder(NULL),
        m_outputFile(NULL),
        m_gotError(false)
{
}

ConsumerThread::~ConsumerThread()
{
    if (m_VideoEncoder)
    {
        if (DO_STAT)
             m_VideoEncoder->printProfilingStats(std::cout);
        delete m_VideoEncoder;
    }

    if (m_outputFile)
        delete m_outputFile;
}

bool ConsumerThread::threadInitialize()
{
    // Create the FrameConsumer.
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    // Create Video Encoder
    if (!createVideoEncoder())
        ORIGINATE_ERROR("Failed to create video m_VideoEncoderoder");

    // Create output file
    m_outputFile = new std::ofstream(OUTPUT_FILENAME.c_str());
    if (!m_outputFile)
        ORIGINATE_ERROR("Failed to open output file.");

    // Stream on
    int e = m_VideoEncoder->output_plane.setStreamStatus(true);
    if (e < 0)
        ORIGINATE_ERROR("Failed to stream on output plane");
    e = m_VideoEncoder->capture_plane.setStreamStatus(true);
    if (e < 0)
        ORIGINATE_ERROR("Failed to stream on capture plane");

    // Set video encoder callback
    m_VideoEncoder->capture_plane.setDQThreadCallback(encoderCapturePlaneDqCallback);

    // startDQThread starts a thread internally which calls the
    // encoderCapturePlaneDqCallback whenever a buffer is dequeued
    // on the plane
    m_VideoEncoder->capture_plane.startDQThread(this);

    // Enqueue all the empty capture plane buffers
    for (uint32_t i = 0; i < m_VideoEncoder->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        CHECK_ERROR(m_VideoEncoder->capture_plane.qBuffer(v4l2_buf, NULL));
    }

    return true;
}

bool ConsumerThread::threadExecute()
{
    IStream *iStream = interface_cast<IStream>(m_stream);
    IFrameConsumer *iFrameConsumer = interface_cast<IFrameConsumer>(m_consumer);

    // Wait until the producer has connected to the stream.
    CONSUMER_PRINT("Waiting until producer is connected...\n");
    if (iStream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Stream failed to connect.");
    CONSUMER_PRINT("Producer has connected; continuing.\n");

    int bufferIndex;

    bufferIndex = 0;

    // Keep acquire frames and queue into encoder
    while (!m_gotError)
    {
        NvBuffer *buffer;
        int fd = -1;

        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.m.planes = planes;

        // Check if we need dqBuffer first
        if (bufferIndex < MAX_ENCODER_FRAMES &&
            m_VideoEncoder->output_plane.getNumQueuedBuffers() <
            m_VideoEncoder->output_plane.getNumBuffers())
        {
            // The queue is not full, no need to dqBuffer
            // Prepare buffer index for the following qBuffer
            v4l2_buf.index = bufferIndex++;
        }
        else
        {
            // Output plane full or max outstanding number reached
            CHECK_ERROR(m_VideoEncoder->output_plane.dqBuffer(v4l2_buf, &buffer,
                                                              NULL, 10));
            // Release the frame.
            fd = v4l2_buf.m.planes[0].m.fd;
            NvBufferDestroy(fd);
            if (VERBOSE_ENABLE)
                CONSUMER_PRINT("Released frame. %d\n", fd);
        }

        // Acquire a frame.
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame());
        IFrame *iFrame = interface_cast<IFrame>(frame);
        if (!iFrame)
        {
            // Send EOS
            v4l2_buf.m.planes[0].m.fd = fd;
            v4l2_buf.m.planes[0].bytesused = 0;
            CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, NULL));
            break;
        }

        // Get the IImageNativeBuffer extension interface and create the fd.
        NV::IImageNativeBuffer *iNativeBuffer =
            interface_cast<NV::IImageNativeBuffer>(iFrame->getImage());
        if (!iNativeBuffer)
            ORIGINATE_ERROR("IImageNativeBuffer not supported by Image.");
        fd = iNativeBuffer->createNvBuffer(STREAM_SIZE,
                                           NvBufferColorFormat_YUV420,
                                           NvBufferLayout_BlockLinear);
        if (VERBOSE_ENABLE)
            CONSUMER_PRINT("Acquired Frame. %d\n", fd);

        // Push the frame into V4L2.
        v4l2_buf.m.planes[0].m.fd = fd;
        v4l2_buf.m.planes[0].bytesused = 1; // byteused must be non-zero
        CHECK_ERROR(m_VideoEncoder->output_plane.qBuffer(v4l2_buf, NULL));
    }

    // Wait till capture plane DQ Thread finishes
    // i.e. all the capture plane buffers are dequeued
    m_VideoEncoder->capture_plane.waitForDQThread(2000);

    CONSUMER_PRINT("Done.\n");

    requestShutdown();

    return true;
}

bool ConsumerThread::threadShutdown()
{
    return true;
}

bool ConsumerThread::createVideoEncoder()
{
    int ret = 0;

    m_VideoEncoder = NvVideoEncoder::createVideoEncoder("enc0");
    if (!m_VideoEncoder)
        ORIGINATE_ERROR("Could not create m_VideoEncoderoder");

    if (DO_STAT)
        m_VideoEncoder->enableProfiling();

    ret = m_VideoEncoder->setCapturePlaneFormat(ENCODER_PIXFMT, STREAM_SIZE.width,
                                    STREAM_SIZE.height, 2 * 1024 * 1024);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set capture plane format");

    ret = m_VideoEncoder->setOutputPlaneFormat(V4L2_PIX_FMT_YUV420M, STREAM_SIZE.width,
                                    STREAM_SIZE.height);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set output plane format");

    ret = m_VideoEncoder->setBitrate(4 * 1024 * 1024);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set bitrate");

    if (ENCODER_PIXFMT == V4L2_PIX_FMT_H264)
    {
        ret = m_VideoEncoder->setProfile(V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
    }
    else
    {
        ret = m_VideoEncoder->setProfile(V4L2_MPEG_VIDEO_H265_PROFILE_MAIN);
    }
    if (ret < 0)
        ORIGINATE_ERROR("Could not set m_VideoEncoderoder profile");

    if (ENCODER_PIXFMT == V4L2_PIX_FMT_H264)
    {
        ret = m_VideoEncoder->setLevel(V4L2_MPEG_VIDEO_H264_LEVEL_5_0);
        if (ret < 0)
            ORIGINATE_ERROR("Could not set m_VideoEncoderoder level");
    }

    ret = m_VideoEncoder->setRateControlMode(V4L2_MPEG_VIDEO_BITRATE_MODE_CBR);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set rate control mode");

    ret = m_VideoEncoder->setIFrameInterval(30);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set I-frame interval");

    ret = m_VideoEncoder->setFrameRate(30, 1);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set m_VideoEncoderoder framerate");

    // Query, Export and Map the output plane buffers so that we can read
    // raw data into the buffers
    ret = m_VideoEncoder->output_plane.setupPlane(V4L2_MEMORY_DMABUF, 10, true, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup output plane");

    // Query, Export and Map the output plane buffers so that we can write
    // m_VideoEncoderoded data from the buffers
    ret = m_VideoEncoder->capture_plane.setupPlane(V4L2_MEMORY_MMAP, 10, true, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup capture plane");

    printf("create vidoe encoder return true\n");
    return true;
}

void ConsumerThread::abort()
{
    m_VideoEncoder->abort();
    m_gotError = true;
}

bool ConsumerThread::encoderCapturePlaneDqCallback(struct v4l2_buffer *v4l2_buf,
                                                   NvBuffer * buffer,
                                                   NvBuffer * shared_buffer,
                                                   void *arg)
{
    ConsumerThread *thiz = (ConsumerThread*)arg;

    if (!v4l2_buf)
    {
        thiz->abort();
        ORIGINATE_ERROR("Failed to dequeue buffer from encoder capture plane");
    }

    thiz->m_outputFile->write((char *) buffer->planes[0].data,
                              buffer->planes[0].bytesused);

    thiz->m_VideoEncoder->capture_plane.qBuffer(*v4l2_buf, NULL);

    // GOT EOS from m_VideoEncoderoder. Stop dqthread.
    if (buffer->planes[0].bytesused == 0)
    {
        CONSUMER_PRINT("Got EOS, exiting...\n");
        return false;
    }

    return true;
}

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates an OutputStream to output to a
 *   FrameConsumer, then performs repeating capture requests for CAPTURE_TIME
 *   seconds before closing the producer and Argus driver.
 ******************************************************************************/
static bool execute()
{
    // Create the CameraProvider object and get the core interface.
    UniqueObj<CameraProvider> cameraProvider = UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to create CameraProvider");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    // Create the capture session using the first device and get the core interface.
    UniqueObj<CaptureSession> captureSession(
            iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    // Create the OutputStream.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(iCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings = interface_cast<IOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        ORIGINATE_ERROR("Failed to get IOutputStreamSettings interface");

    iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iStreamSettings->setResolution(STREAM_SIZE);
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));

    // Launch the FrameConsumer thread to consume frames from the OutputStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    ConsumerThread frameConsumerThread(outputStream.get());
    PROPAGATE_ERROR(frameConsumerThread.initialize());

    // Wait until the consumer is connected to the stream.
    PROPAGATE_ERROR(frameConsumerThread.waitRunning());

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());

    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get ISourceSettings interface");
    iSourceSettings->setFrameDurationRange(Range<uint64_t>(1e9/DEFAULT_FPS));

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for CAPTURE_TIME seconds.
    for (int i = 0; i < CAPTURE_TIME && !frameConsumerThread.isInError(); i++)
        sleep(1);

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the output stream to end the consumer thread.
    outputStream.reset();

    // Wait for the consumer thread to complete.
    PROPAGATE_ERROR(frameConsumerThread.shutdown());

    PRODUCER_PRINT("Done -- exiting.\n");

    return true;
}

}; // namespace ArgusSamples


static void printHelp()
{
    printf("Usage: camera_recording [OPTIONS]\n"
           "Options:\n"
           "  -r        Set output resolution WxH [Default 640x480]\n"
           "  -f        Set output filename [Default output.h264]\n"
           "  -t        Set encoder type H264 or H265 [Default H264]\n"
           "  -d        Set capture duration [Default 5 seconds]\n"
           "  -s        Enable profiling\n"
           "  -v        Enable verbose message\n"
           "  -h        Print this help\n");
}

static bool parseCmdline(int argc, char **argv)
{
    int c, w, h;
    bool haveFilename = false;
    while ((c = getopt(argc, argv, "r:f:t:d:s::v::h")) != -1)
    {
        switch (c)
        {
            case 'r':
                if (sscanf(optarg, "%dx%d", &w, &h) != 2)
                    return false;
                STREAM_SIZE.width = w;
                STREAM_SIZE.height = h;
                break;
            case 'f':
                OUTPUT_FILENAME = optarg;
                haveFilename = true;
                break;
            case 't':
                if (strcmp(optarg, "H264") == 0)
                    ENCODER_PIXFMT = V4L2_PIX_FMT_H264;
                else if (strcmp(optarg, "H265") == 0)
                {
                    ENCODER_PIXFMT = V4L2_PIX_FMT_H265;
                    if (!haveFilename)
                        OUTPUT_FILENAME = "output.h265";
                }
                else
                    return false;
                break;
            case 'd':
                CAPTURE_TIME = atoi(optarg);
                break;
            case 's':
                DO_STAT = true;
                break;
            case 'v':
                VERBOSE_ENABLE = true;
                break;
            default:
                return false;
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    if (!parseCmdline(argc, argv))
    {
        printHelp();
        return EXIT_FAILURE;
    }

    NvApplicationProfiler &profiler = NvApplicationProfiler::getProfilerInstance();

    if (!ArgusSamples::execute())
        return EXIT_FAILURE;

    profiler.stop();
    profiler.printProfilerData(std::cout);

    return EXIT_SUCCESS;
}
