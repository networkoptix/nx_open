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
#include "Window.h"
#include "Thread.h"
#include "PreviewConsumer.h"

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>

#include <unistd.h>
#include <stdlib.h>

using namespace Argus;

/*
 * This sample outputs capture requests to two streams, one of which has denoise algorithms
 * enabled while the other does not, and then renders them to a split-screen window.
 */

namespace ArgusSamples
{

// Constants.
static const uint32_t       CAPTURE_TIME    = 10; // In seconds.
static const Size           STREAM_SIZE      (640, 480);
static const NormalizedRect SOURCE_CLIP_RECT (0.4f, 0.4f, 0.6f, 0.6f);

// Globals.
UniqueObj<CameraProvider> g_cameraProvider;
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)

static bool execute()
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(0, 0, STREAM_SIZE.width, STREAM_SIZE.height);
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");

    // Create a capture session using the first available device.
    std::vector<CameraDevice*> cameraDevices;
    if (iCameraProvider->getCameraDevices(&cameraDevices) != STATUS_OK)
        ORIGINATE_ERROR("Failed to get CameraDevices");
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No CameraDevices available");
    UniqueObj<CaptureSession> captureSession(
        iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Create two output streams, one for unprocessed preview and one for denoise.
    PRODUCER_PRINT("Creating output streams\n");
    UniqueObj<OutputStreamSettings> streamSettings(iCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings = interface_cast<IOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");
    iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iStreamSettings->setResolution(STREAM_SIZE);
    iStreamSettings->setEGLDisplay(g_display.get());
    UniqueObj<OutputStream> previewStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IStream *iPreviewStream = interface_cast<IStream>(previewStream);
    if (!iPreviewStream)
        ORIGINATE_ERROR("Failed to create preview stream");
    UniqueObj<OutputStream> denoiseStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IStream *iDenoiseStream = interface_cast<IStream>(denoiseStream);
    if (!iDenoiseStream)
        ORIGINATE_ERROR("Failed to create denoise stream");

    // Connect a PreviewConsumer to the streams to render a split-screen, side-by-side rendering.
    PRODUCER_PRINT("Launching consumer thread\n");
    std::vector<EGLStreamKHR> eglStreams;
    eglStreams.push_back(iPreviewStream->getEGLStream());
    eglStreams.push_back(iDenoiseStream->getEGLStream());
    PreviewConsumerThread consumerThread(g_display.get(), eglStreams,
                                         PreviewConsumerThread::LAYOUT_SPLIT_VERTICAL,
                                         true /* Sync stream frames */);
    PROPAGATE_ERROR(consumerThread.initialize());
    consumerThread.setLineWidth(1);
    consumerThread.setLineColor(1.0f, 0.0f, 0.0f);

    // Wait until the consumer is connected to the streams.
    PROPAGATE_ERROR(consumerThread.waitRunning());

    // Create capture request and enable output streams.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(previewStream.get());
    iRequest->enableOutputStream(denoiseStream.get());

    // Use small source clip rects to zoom the image to make noise more visible.
    IStreamSettings *previewStreamSettings =
        interface_cast<IStreamSettings>(iRequest->getStreamSettings(previewStream.get()));
    if (!previewStreamSettings)
        ORIGINATE_ERROR("Failed to get preview stream settings interface");
    IStreamSettings *denoiseStreamSettings =
        interface_cast<IStreamSettings>(iRequest->getStreamSettings(denoiseStream.get()));
    if (!denoiseStreamSettings)
        ORIGINATE_ERROR("Failed to get denoise stream settings interface");
    previewStreamSettings->setSourceClipRect(SOURCE_CLIP_RECT);
    denoiseStreamSettings->setSourceClipRect(SOURCE_CLIP_RECT);

    // Enable denoise for the request.
    IDenoiseSettings *denoiseSettings = interface_cast<IDenoiseSettings>(request);
    if (!denoiseSettings)
        ORIGINATE_ERROR("Failed to get DenoiseSettings interface");
    denoiseSettings->setDenoiseMode(DENOISE_MODE_FAST);
    denoiseSettings->setDenoiseStrength(1.0f);

    // Disable all post-processing (including denoise) for the preview stream (enabled by default).
    previewStreamSettings->setPostProcessingEnable(false);

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for CAPTURE_TIME seconds.
    PROPAGATE_ERROR(window.pollingSleep(CAPTURE_TIME));

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the output streams and wait for the consumer thread to complete.
    previewStream.reset();
    denoiseStream.reset();
    PROPAGATE_ERROR(consumerThread.shutdown());

    // Shut down Argus.
    g_cameraProvider.reset();

    // Shut down the window (destroys window's EGLSurface).
    window.shutdown();

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
