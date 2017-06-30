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
#include "EGLGlobal.h"
#include "GLContext.h"
#include "JPEGConsumer.h"
#include "PreviewConsumer.h"
#include "Window.h"
#include "Thread.h"

#include <Argus/Argus.h>

#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>

using namespace Argus;

namespace ArgusSamples
{

// Constants.
static const uint32_t CAPTURE_TIME  = 5; // In seconds.
static const Size     PREVIEW_STREAM_SIZE(640, 480);
static const uint32_t NUMBER_BURST_CAPTURES = 3;

// Globals and derived constants.
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...)         printf("PRODUCER: " __VA_ARGS__)

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates two OutputStreams -- one for live
 *   preview to display and the other to write JPEG files -- and submits capture
 *   requests. Burst captures are used such that the JPEG stream is only written
 *   to once for every NUMBER_BURST_CAPTURES captures.
 ******************************************************************************/
static bool execute()
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    // Use the first device.
    ICameraProperties *iCameraDevice = interface_cast<ICameraProperties>(cameraDevices[0]);
    if (!iCameraDevice)
        ORIGINATE_ERROR("Failed to get camera device properties");
    std::vector<Argus::SensorMode*> sensorModes;
    iCameraDevice->getSensorModes(&sensorModes);
    if (!sensorModes.size())
        ORIGINATE_ERROR("Failed to get valid sensor mode list.");

    // Create the capture session.
    UniqueObj<CaptureSession> captureSession(
            iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Create the stream settings and set the common properties.
    UniqueObj<OutputStreamSettings> streamSettings(iCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings = interface_cast<IOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");
    iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iStreamSettings->setEGLDisplay(g_display.get());

    // Create preview-sized OutputStream that is consumed by the preview (OpenGL) consumer.
    PRODUCER_PRINT("Creating preview output stream\n");
    iStreamSettings->setResolution(PREVIEW_STREAM_SIZE);
    UniqueObj<OutputStream> previewStream(
            iCaptureSession->createOutputStream(streamSettings.get()));
    IStream *iPreviewStream = interface_cast<IStream>(previewStream);
    if (!iPreviewStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    PRODUCER_PRINT("Launching preview consumer thread\n");
    PreviewConsumerThread previewConsumerThread(iPreviewStream->getEGLDisplay(),
                                                iPreviewStream->getEGLStream());
    PROPAGATE_ERROR(previewConsumerThread.initialize());
    PROPAGATE_ERROR(previewConsumerThread.waitRunning());

    // Create a full-resolution OutputStream that is consumed by the JPEG Consumer.
    PRODUCER_PRINT("Creating JPEG output stream\n");
    ISensorMode *mode = interface_cast<ISensorMode>(sensorModes[0]);
    if (!mode)
        ORIGINATE_ERROR("Failed to get sensor mode.");
    iStreamSettings->setResolution(mode->getResolution());
    UniqueObj<OutputStream> jpegStream(iCaptureSession->createOutputStream(streamSettings.get()));
    if (!jpegStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    PRODUCER_PRINT("Launching JPEG consumer thread\n");
    JPEGConsumerThread jpegConsumerThread(jpegStream.get());
    PROPAGATE_ERROR(jpegConsumerThread.initialize());
    PROPAGATE_ERROR(jpegConsumerThread.waitRunning());

    // Create the capture requests.
    UniqueObj<Request> requests[NUMBER_BURST_CAPTURES];
    std::vector<const Argus::Request*> requestVec;
    for (uint32_t i = 0; i < NUMBER_BURST_CAPTURES; i++)
    {
        requests[i] = UniqueObj<Request>(iCaptureSession->createRequest());
        IRequest *iRequest = interface_cast<IRequest>(requests[i]);
        if (!iRequest)
            ORIGINATE_ERROR("Failed to create Request");
        requestVec.push_back(requests[i].get());

        // Enable the preview stream for every capture in the burst.
        iRequest->enableOutputStream(previewStream.get());

        // Enable the JPEG stream for only the first capture in the burst.
        if (i == 0)
            iRequest->enableOutputStream(jpegStream.get());
    }

    if (iCaptureSession->repeatBurst(requestVec) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat burst capture request");

    // Wait for CAPTURE_TIME seconds.
    PROPAGATE_ERROR(window.pollingSleep(CAPTURE_TIME));

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the output streams (stops consumer threads).
    previewStream.reset();
    jpegStream.reset();

    // Wait for the consumer threads to complete.
    PROPAGATE_ERROR(previewConsumerThread.shutdown());
    PROPAGATE_ERROR(jpegConsumerThread.shutdown());

    // Shut down Argus.
    cameraProvider.reset();

    // Cleanup the EGL display
    PROPAGATE_ERROR(g_display.cleanup());

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
