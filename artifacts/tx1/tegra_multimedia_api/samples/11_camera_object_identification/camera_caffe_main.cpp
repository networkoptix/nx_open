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
#include "camera_caffe_main.h"

using namespace Argus;
using namespace EGLStream;

static camera_caffe_context ctx;

// Constant configuration.
#define MAX_PENDING_FRAMES      (3)
#define DEFAULT_FPS             (30)

// Configurations
static int          CAPTURE_TIME = 30; // In seconds.
static bool         DO_STAT = false;

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
 *   buffers to V4L2 for video conversion. The converter will feed the image to
 *   processing routine.
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

    bool createImageConverter();
    void abort();

    static bool converterCapturePlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer,
            void *arg);
    static bool converterOutputPlaneDqCallback(
            struct v4l2_buffer *v4l2_buf,
            NvBuffer *buffer,
            NvBuffer *shared_buffer,
            void *arg);

    void writeFrameToOpencvConsumer(
        camera_caffe_context *p_ctx,
        NvBuffer *buffer);

    OutputStream* m_stream;
    UniqueObj<FrameConsumer> m_consumer;
    NvVideoConverter *m_ImageConverter;
    std::queue < NvBuffer * > *m_ConvOutputPlaneBufQueue;
    pthread_mutex_t m_queueLock;
    pthread_cond_t m_queueCond;
    int conv_buf_num;
    int m_numPendingFrames;

    camera_caffe_context *m_pContext;

    bool m_gotError;
};

ConsumerThread::ConsumerThread(OutputStream* stream) :
        m_stream(stream),
        m_ImageConverter(NULL),
        m_gotError(false)
{
    conv_buf_num = FRAME_CONVERTER_BUF_NUMBER;
    m_ConvOutputPlaneBufQueue = new std::queue < NvBuffer * >;
    pthread_mutex_init(&m_queueLock, NULL);
    pthread_cond_init(&m_queueCond, NULL);
    m_pContext = &ctx;
    m_numPendingFrames = 0;
}

ConsumerThread::~ConsumerThread()
{
    delete m_ConvOutputPlaneBufQueue;
    if (m_ImageConverter)
    {
        if (DO_STAT)
             m_ImageConverter->printProfilingStats(std::cout);
        delete m_ImageConverter;
    }
}

bool ConsumerThread::threadInitialize()
{
    // Create the FrameConsumer.
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

    // Create Video converter
    if (!createImageConverter())
        ORIGINATE_ERROR("Failed to create video m_ImageConverteroder");

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

    // Keep acquire frames and queue into converter
    while (!m_gotError)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.m.planes = planes;

        pthread_mutex_lock(&m_queueLock);
        while (!m_gotError &&
            ((m_ConvOutputPlaneBufQueue->empty()) || (m_numPendingFrames >= MAX_PENDING_FRAMES)))
        {
            pthread_cond_wait(&m_queueCond, &m_queueLock);
        }

        if (m_gotError)
        {
            pthread_mutex_unlock(&m_queueLock);
            break;
        }

        NvBuffer *buffer = NULL;
        int fd = -1;

        buffer = m_ConvOutputPlaneBufQueue->front();
        m_ConvOutputPlaneBufQueue->pop();
        pthread_mutex_unlock(&m_queueLock);

        // Acquire a frame.
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame());
        IFrame *iFrame = interface_cast<IFrame>(frame);
        if (!iFrame)
            break;

        // Get the IImageNativeBuffer extension interface and create the fd.
        NV::IImageNativeBuffer *iNativeBuffer =
            interface_cast<NV::IImageNativeBuffer>(iFrame->getImage());
        if (!iNativeBuffer)
            ORIGINATE_ERROR("IImageNativeBuffer not supported by Image.");
        fd = iNativeBuffer->createNvBuffer(Size(ctx.width, ctx.height),
                                           NvBufferColorFormat_YUV420,
                                           NvBufferLayout_BlockLinear);

        // Push the frame into V4L2.
        v4l2_buf.index = buffer->index;
        // Set the bytesused to some non-zero value so that
        // v4l2 convert processes the buffer
        buffer->planes[0].bytesused = 1;
        buffer->planes[0].fd = fd;
        buffer->planes[1].fd = fd;
        buffer->planes[2].fd = fd;
        pthread_mutex_lock(&m_queueLock);
        m_numPendingFrames++;
        pthread_mutex_unlock(&m_queueLock);

        CONSUMER_PRINT("acquireFd %d (%d frames)\n", fd, m_numPendingFrames);

        int ret = m_ImageConverter->output_plane.qBuffer(v4l2_buf, buffer);
        if (ret < 0) {
            abort();
            ORIGINATE_ERROR("Fail to qbuffer for conv output plane");
        }
    }

    // Wait till capture plane DQ Thread finishes
    // i.e. all the capture plane buffers are dequeued
    m_ImageConverter->capture_plane.waitForDQThread(2000);

    CONSUMER_PRINT("Done.\n");

    requestShutdown();

    return true;
}

bool ConsumerThread::threadShutdown()
{
    return true;
}

void ConsumerThread::writeFrameToOpencvConsumer(
    camera_caffe_context *p_ctx, NvBuffer *buffer)
{
    NvBuffer::NvBufferPlane *plane = &buffer->planes[0];
    uint8_t *pdata = (uint8_t *) plane->data;

    // output RGB frame to opencv, only 1 plane
    if (p_ctx->lib_handler)
    {
        p_ctx->opencv_img_processing(p_ctx->opencv_handler
            , pdata, plane->fmt.width, plane->fmt.height);
    }
}

bool ConsumerThread::converterCapturePlaneDqCallback(
    struct v4l2_buffer *v4l2_buf,
    NvBuffer * buffer,
    NvBuffer * shared_buffer,
    void *arg)
{
    ConsumerThread *thiz = (ConsumerThread*)arg;
    camera_caffe_context *p_ctx = thiz->m_pContext;
    int e;

    if (!v4l2_buf)
    {
        REPORT_ERROR("Failed to dequeue buffer from conv capture plane");
        thiz->abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }

    thiz->writeFrameToOpencvConsumer(p_ctx, buffer);

    e = thiz->m_ImageConverter->capture_plane.qBuffer(*v4l2_buf, NULL);
    if (e < 0)
        ORIGINATE_ERROR("qBuffer failed");

    return true;
}

bool ConsumerThread::converterOutputPlaneDqCallback(
    struct v4l2_buffer *v4l2_buf,
    NvBuffer * buffer,
    NvBuffer * shared_buffer,
    void *arg)
{
    ConsumerThread *thiz = (ConsumerThread*)arg;

    if (!v4l2_buf)
    {
        REPORT_ERROR("Failed to dequeue buffer from conv capture plane");
        thiz->abort();
        return false;
    }

    if (v4l2_buf->m.planes[0].bytesused == 0)
    {
        return false;
    }
    NvBufferDestroy(shared_buffer->planes[0].fd);

    CONSUMER_PRINT("releaseFd %d (%d frames)\n", shared_buffer->planes[0].fd, thiz->m_numPendingFrames);
    pthread_mutex_lock(&thiz->m_queueLock);
    thiz->m_numPendingFrames--;
    thiz->m_ConvOutputPlaneBufQueue->push(buffer);
    pthread_cond_broadcast(&thiz->m_queueCond);
    pthread_mutex_unlock(&thiz->m_queueLock);

    return true;
}

bool ConsumerThread::createImageConverter()
{
    int ret = 0;

    // YUV420 --> RGB32 converter
    m_ImageConverter = NvVideoConverter::createVideoConverter("conv");
    if (!m_ImageConverter)
        ORIGINATE_ERROR("Could not create m_ImageConverteroder");

    if (DO_STAT)
        m_ImageConverter->enableProfiling();

    m_ImageConverter->capture_plane.
        setDQThreadCallback(converterCapturePlaneDqCallback);
    m_ImageConverter->output_plane.
        setDQThreadCallback(converterOutputPlaneDqCallback);

    ret = m_ImageConverter->setOutputPlaneFormat(V4L2_PIX_FMT_YUV420M, m_pContext->width,
                                    m_pContext->height, V4L2_NV_BUFFER_LAYOUT_BLOCKLINEAR);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set output plane format");

    ret = m_ImageConverter->setCapturePlaneFormat(V4L2_PIX_FMT_ABGR32, m_pContext->width,
                                    m_pContext->height, V4L2_NV_BUFFER_LAYOUT_PITCH);
    if (ret < 0)
        ORIGINATE_ERROR("Could not set capture plane format");

    // Query, Export and Map the output plane buffers so that we can read
    // raw data into the buffers
    ret = m_ImageConverter->output_plane.setupPlane(V4L2_MEMORY_DMABUF, conv_buf_num, false, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup output plane");

    // Query, Export and Map the output plane buffers so that we can write
    // m_ImageConverteroded data from the buffers
    ret = m_ImageConverter->capture_plane.setupPlane(V4L2_MEMORY_MMAP, conv_buf_num, true, false);
    if (ret < 0)
        ORIGINATE_ERROR("Could not setup capture plane");

    // Add all empty conv output plane buffers to m_ConvOutputPlaneBufQueue
    for (uint32_t i = 0; i < m_ImageConverter->output_plane.getNumBuffers(); i++)
    {
        m_ConvOutputPlaneBufQueue->push(
            m_ImageConverter->output_plane.getNthBuffer(i));
    }

    // conv output plane STREAMON
    ret = m_ImageConverter->output_plane.setStreamStatus(true);
    if (ret < 0)
        ORIGINATE_ERROR("fail to set conv output stream on");

    // conv capture plane STREAMON
    ret = m_ImageConverter->capture_plane.setStreamStatus(true);
    if (ret < 0)
        ORIGINATE_ERROR("fail to set conv capture stream on");

    // Start threads to dequeue buffers on conv capture plane,
    // conv output plane and capture plane
    m_ImageConverter->capture_plane.startDQThread(this);
    m_ImageConverter->output_plane.startDQThread(this);

    // Enqueue all empty conv capture plane buffers
    for (uint32_t i = 0; i < m_ImageConverter->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, MAX_PLANES * sizeof(struct v4l2_plane));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;

        ret = m_ImageConverter->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0) {
            abort();
            ORIGINATE_ERROR("Error queueing buffer at conv capture plane");
        }
    }

    printf("create vidoe converter return true\n");
    return true;
}

void ConsumerThread::abort()
{
    m_ImageConverter->abort();
    m_gotError = true;
}

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates an OutputStream to output to a
 *   FrameConsumer, then performs repeating capture requests for CAPTURE_TIME
 *   seconds before closing the producer and Argus driver.
 ******************************************************************************/
static bool execute(camera_caffe_context *p_ctx)
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
    if (iStreamSettings)
    {
        iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iStreamSettings->setResolution(Size(p_ctx->width, p_ctx->height));
    }
    else
    {
        ORIGINATE_ERROR("NULL for output stream settings!");
    }
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

static int init_ctx(camera_caffe_context *p_ctx)
{
    // opencv processor initialize
    p_ctx->lib_handler = dlopen(p_ctx->lib_file, RTLD_LAZY);
    if (!p_ctx->lib_handler)
    {
        printf("Fail to open OPENCV LIB, continue to run without processing\n");
    }
    else
    {
        p_ctx->opencv_handler_open = (opencv_handler_open_t)
            dlsym(p_ctx->lib_handler, OPENCV_HANDLER_OPEN_ENTRY);
        if (!p_ctx->opencv_handler_open)
            ORIGINATE_ERROR("Fail to get sym opencv_handler_open");
        p_ctx->opencv_handler_close = (opencv_handler_close_t)
            dlsym(p_ctx->lib_handler, OPENCV_HANDLER_CLOSE_ENTRY);
        if (!p_ctx->opencv_handler_close)
            ORIGINATE_ERROR("Fail to get sym opencv_handler_close");
        p_ctx->opencv_img_processing = (opencv_img_processing_t)
            dlsym(p_ctx->lib_handler, OPENCV_IMG_PROCESSING_ENTRY);
        if (!p_ctx->opencv_img_processing)
            ORIGINATE_ERROR("Fail to get sym opencv_img_processing");
        p_ctx->opencv_set_config = (opencv_set_config_t)
            dlsym(p_ctx->lib_handler, OPENCV_SET_CONFIG_ENTRY);
        if (!p_ctx->opencv_set_config)
            ORIGINATE_ERROR("Fail to get sym opencv_set_config");

        p_ctx->opencv_handler = p_ctx->opencv_handler_open();
        if (!p_ctx->opencv_handler)
            ORIGINATE_ERROR("Fail to opencv_handler_open");

        // configure opencv consumer
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_IMGWIDTH, &p_ctx->width);
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_IMGHEIGHT, &p_ctx->height);
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_CAFFE_MODELFILE, p_ctx->model_file);
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_CAFFE_TRAINEDFILE, p_ctx->trained_file);
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_CAFFE_MEANFILE, p_ctx->mean_file);
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_CAFFE_LABELFILE, p_ctx->label_file);
        p_ctx->opencv_set_config(p_ctx->opencv_handler
            , OPENCV_CONSUMER_CONFIG_START, NULL);
    }

    return 0;
}


static void deinit_ctx(camera_caffe_context *p_ctx)
{
    if (p_ctx)
    {
        if (p_ctx->lib_handler)
        {
            p_ctx->opencv_handler_close(p_ctx->opencv_handler);
            dlclose(p_ctx->lib_handler);
        }
        if (p_ctx->lib_file)
            free(p_ctx->lib_file);
        if (p_ctx->model_file)
            free(p_ctx->model_file);
        if (p_ctx->mean_file)
            free(p_ctx->mean_file);
        if (p_ctx->label_file)
            free(p_ctx->label_file);
    }
    return;
}


static void set_default(camera_caffe_context *p_ctx)
{
    memset(p_ctx, 0, sizeof(camera_caffe_context));
    return;
}


int main(int argc, const char *argv[])
{
    int ret = EXIT_SUCCESS;
    camera_caffe_context *p_ctx = &ctx;

    set_default(p_ctx);
    ret = parse_csv_args(p_ctx, argc, argv);
    if (ret < 0)
        ORIGINATE_ERROR("Fail to parse arguments.");
    ret = init_ctx(p_ctx);
    if (ret < 0)
        ORIGINATE_ERROR("Fail to initialize context.");

    if (!ArgusSamples::execute(p_ctx))
        return EXIT_FAILURE;

    deinit_ctx(p_ctx);

    return ret;
}
