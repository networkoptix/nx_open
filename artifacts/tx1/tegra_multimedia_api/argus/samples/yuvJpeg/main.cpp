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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

using namespace Argus;
using namespace EGLStream;

namespace ArgusSamples
{

// Constant configuration.
static const uint32_t CAPTURE_TIME  = 1; // In seconds.
static const Size     STREAM_SIZE   (640, 480);

#ifdef ANDROID
#define JPEG_PREFIX "/sdcard/DCIM/Argus_"
#else
#define JPEG_PREFIX ""
#endif

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

/*******************************************************************************
 * FrameConsumer thread:
 *   Creates a FrameConsumer object to read frames from the OutputStream, then
 *   acquires and prints frame info from the IImage and IImage2D interfaces
 *   while frames are presented. It will also write JPEG files to disk using the
 *   IImageJPEG interface.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(OutputStream* stream) :
        m_stream(stream)
    {
    }
    ~ConsumerThread()
    {
    }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    OutputStream* m_stream;
    UniqueObj<FrameConsumer> m_consumer;
};

bool ConsumerThread::threadInitialize()
{
    // Create the FrameConsumer.
    m_consumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_stream));
    if (!m_consumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer");

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

    while (true)
    {
        UniqueObj<Frame> frame(iFrameConsumer->acquireFrame());
        if (!frame)
            break;

        // Use the IFrame interface to print out the frame number/timestamp, and
        // to provide access to the Image in the Frame.
        IFrame *iFrame = interface_cast<IFrame>(frame);
        if (!iFrame)
            ORIGINATE_ERROR("Failed to get IFrame interface.");
        CONSUMER_PRINT("Acquired Frame: %llu, time %llu\n",
                       static_cast<unsigned long long>(iFrame->getNumber()),
                       static_cast<unsigned long long>(iFrame->getTime()));

        // Print out some capture metadata from the frame.
        IArgusCaptureMetadata *iArgusCaptureMetadata = interface_cast<IArgusCaptureMetadata>(frame);
        if (!iArgusCaptureMetadata)
            ORIGINATE_ERROR("Failed to get IArgusCaptureMetadata interface.");
        CaptureMetadata *metadata = iArgusCaptureMetadata->getMetadata();
        ICaptureMetadata *iMetadata = interface_cast<ICaptureMetadata>(metadata);
        if (!iMetadata)
            ORIGINATE_ERROR("Failed to get ICaptureMetadata interface.");
        CONSUMER_PRINT("\tSensor Timestamp: %llu, LUX: %f\n",
                       static_cast<unsigned long long>(iMetadata->getSensorTimestamp()),
                       iMetadata->getSceneLux());

        // Print out image details, and map the buffers to read out some data.
        Image *image = iFrame->getImage();
        IImage *iImage = interface_cast<IImage>(image);
        IImage2D *iImage2D = interface_cast<IImage2D>(image);
        for (uint32_t i = 0; i < iImage->getBufferCount(); i++)
        {
            const uint8_t *d = static_cast<const uint8_t*>(iImage->mapBuffer(i));
            if (!d)
                ORIGINATE_ERROR("\tFailed to map buffer\n");

            Size size = iImage2D->getSize(i);
            CONSUMER_PRINT("\tIImage(2D): "
                           "buffer %u (%ux%u, %u stride), "
                           "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                           i, size.width, size.height, iImage2D->getStride(i),
                           d[0], d[1], d[2], d[3], d[4], d[5],
                           d[6], d[7], d[8], d[9], d[10], d[11]);
        }

        // Write a JPEG to disk.
        IImageJPEG *iJPEG = interface_cast<IImageJPEG>(image);
        if (iJPEG)
        {
            char path[256];
            snprintf(path, sizeof(path), JPEG_PREFIX"%llu.JPG",
                     static_cast<unsigned long long>(iFrame->getNumber()));
            Argus::Status status = iJPEG->writeJPEG(path);
            if (status == STATUS_OK)
                CONSUMER_PRINT("\tWrote JPEG: %s\n", path);
            else
                CONSUMER_PRINT("\tFailed to write JPEG: %s (status %d)\n", path, status);
        }
    }

    CONSUMER_PRINT("Done.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool ConsumerThread::threadShutdown()
{
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
    if (iStreamSettings)
    {
        iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iStreamSettings->setResolution(STREAM_SIZE);
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

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for CAPTURE_TIME seconds.
    sleep(CAPTURE_TIME);

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

int main(int argc, const char *argv[])
{
    if (!ArgusSamples::execute())
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
