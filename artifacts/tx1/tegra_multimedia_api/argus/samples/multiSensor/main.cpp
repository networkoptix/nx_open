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

/*
 * This sample opens two independent camera sessions using 2 sensors it then uses the first sensor
 * to display a preview on the screen, while taking jpeg snapshots every second from the second
 * sensor. The Jpeg saving and Preview consumption happen on two consumer threads in the
 * PreviewConsumerThread and JPEGConsumerThread classes, located in the util folder.
 */

namespace ArgusSamples
{
// Constants.
static const uint32_t CAPTURE_TIME  = 5; // In seconds.
static const Size     PREVIEW_STREAM_SIZE(640, 480);
static const uint32_t NUMBER_SESSIONS = 2;

// Globals and derived constants.
UniqueObj<CameraProvider> g_cameraProvider;
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)

static bool execute()
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());

    // Get the ICameraProvider interface from the global CameraProvider.
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    if (cameraDevices.size() < NUMBER_SESSIONS)
    {
        ORIGINATE_ERROR("Insufficient number of sensors present cannot run multisensor sample");
    }

    // Get the second cameras properties since it is used for the storage session.
    ICameraProperties *iCameraDevice = interface_cast<ICameraProperties>(cameraDevices[1]);
    if (!iCameraDevice)
    {
        ORIGINATE_ERROR("Failed to get the camera device.");
    }

    std::vector<Argus::SensorMode*> sensorModes;
    iCameraDevice->getSensorModes(&sensorModes);
    if (!sensorModes.size())
    {
        ORIGINATE_ERROR("Failed to get valid sensor mode list.");
    }

    // Create the capture sessions, one will be for storing images and one for preview.
    UniqueObj<CaptureSession> captureSessions[NUMBER_SESSIONS];
    for (uint32_t i = 0; i < NUMBER_SESSIONS; i++)
    {
        captureSessions[i] =
                UniqueObj<CaptureSession>(iCameraProvider->createCaptureSession(cameraDevices[i]));

        if (!captureSessions[i])
            ORIGINATE_ERROR("Failed to create CaptureSession with device %d.", i);
    }

    ICaptureSession *iPreviewCaptureSession = interface_cast<ICaptureSession>(captureSessions[0]);
    ICaptureSession *iStorageCaptureSession = interface_cast<ICaptureSession>(captureSessions[1]);

    if (!iPreviewCaptureSession || !iStorageCaptureSession)
        ORIGINATE_ERROR("Failed to get capture session interfaces");

    // Create streams.
    PRODUCER_PRINT("Creating the preview stream.\n");
    UniqueObj<OutputStreamSettings> previewSettings(
        iPreviewCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iPreviewSettings =
        interface_cast<IOutputStreamSettings>(previewSettings);
    if (iPreviewSettings)
    {
        iPreviewSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iPreviewSettings->setResolution(PREVIEW_STREAM_SIZE);
        iPreviewSettings->setEGLDisplay(g_display.get());
    }
    UniqueObj<OutputStream> previewStream(
            iPreviewCaptureSession->createOutputStream(previewSettings.get()));
    IStream *iPreviewStream = interface_cast<IStream>(previewStream);
    if (!iPreviewStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    PRODUCER_PRINT("Launching preview consumer thread\n");
    PreviewConsumerThread previewConsumer(g_display.get(), iPreviewStream->getEGLStream());
    PROPAGATE_ERROR(previewConsumer.initialize());
    PROPAGATE_ERROR(previewConsumer.waitRunning());

    // Use the 1st sensor mode as the size we want to store.
    ISensorMode *iMode = interface_cast<ISensorMode>(sensorModes[0]);
    if (!iMode)
        ORIGINATE_ERROR("Failed to get the sensor mode.");

    // Create streams.
    PRODUCER_PRINT("Creating the storage stream.\n");
    UniqueObj<OutputStreamSettings> storageSettings(
        iStorageCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStorageSettings =
        interface_cast<IOutputStreamSettings>(storageSettings);
    if (iStorageSettings)
    {
        iStorageSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iStorageSettings->setResolution(iMode->getResolution());
        iStorageSettings->setEGLDisplay(g_display.get());
    }
    UniqueObj<OutputStream> storageStream(
            iStorageCaptureSession->createOutputStream(storageSettings.get()));
    if (!storageStream.get())
        ORIGINATE_ERROR("Failed to create StorageStream");

    JPEGConsumerThread jpegConsumer(storageStream.get());
    PROPAGATE_ERROR(jpegConsumer.initialize());
    PROPAGATE_ERROR(jpegConsumer.waitRunning());

    // Create the two requests
    UniqueObj<Request> previewRequest(iPreviewCaptureSession->createRequest());
    UniqueObj<Request> storageRequest(iStorageCaptureSession->createRequest());
    if (!previewRequest || !storageRequest)
        ORIGINATE_ERROR("Failed to create Request");

    IRequest *iPreviewRequest = interface_cast<IRequest>(previewRequest);
    IRequest *iStorageRequest = interface_cast<IRequest>(storageRequest);
    if (!iPreviewRequest || !iStorageRequest)
        ORIGINATE_ERROR("Failed to create Request interface");

    iPreviewRequest->enableOutputStream(previewStream.get());
    iStorageRequest->enableOutputStream(storageStream.get());

    // Argus is now all setup and ready to capture

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");

    // Start the preview
    if (iPreviewCaptureSession->repeat(previewRequest.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request for preview");

    // Wait for CAPTURE_TIME seconds and do a storage capture every second.
    for (uint32_t i = 0; i < CAPTURE_TIME; i++)
    {
        if (iStorageCaptureSession->repeat(storageRequest.get()) != STATUS_OK)
            ORIGINATE_ERROR("Failed to start repeat capture request for preview");
        PROPAGATE_ERROR(window.pollEvents());
        sleep(1);
    }

    // all done shut down
    iStorageCaptureSession->stopRepeat();
    iPreviewCaptureSession->stopRepeat();
    iStorageCaptureSession->waitForIdle();
    iPreviewCaptureSession->waitForIdle();

    previewStream.reset();
    storageStream.reset();

    // Wait for the consumer threads to complete.
    PROPAGATE_ERROR(previewConsumer.shutdown());
    PROPAGATE_ERROR(jpegConsumer.shutdown());

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
