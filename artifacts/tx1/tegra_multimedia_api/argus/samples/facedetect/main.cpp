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

#include <Argus/Argus.h>
#include <Argus/Ext/FaceDetect.h>
#include <EGLStream/EGLStream.h>

#include <unistd.h>
#include <stdlib.h>

using namespace Argus;

static uint32_t g_cameraIndex = 0;
static Rectangle g_windowRect(0, 0, 640, 480);

namespace ArgusSamples
{

// Constants.
static const uint32_t CAPTURE_TIME  = 10; // In seconds.
static const Size     STREAM_SIZE     (640, 480);

// Globals.
UniqueObj<CameraProvider> g_cameraProvider;
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

/*******************************************************************************
 * GL Consumer thread:
 *   Opens an on-screen window, and renders a live camera preview with
 *   rectangles highlighting detected faces. The intensity of the rectangle
 *   increases as the confidence increases.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(EGLStreamKHR stream) :
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

    EGLStreamKHR m_stream;
    GLContext m_context;
    GLuint m_streamTexture;
    GLuint m_textureProgram;
    GLuint m_faceRectProgram;
};

bool ConsumerThread::threadInitialize()
{
    Window &window = Window::getInstance();

    // Create the context and make it current.
    CONSUMER_PRINT("Creating context.\n");
    PROPAGATE_ERROR(m_context.initialize(&window));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Create the shader program to render the texture. It uses vertex attrib
    // array 0 to store the constant quad coordinates.
    {
        static const char vtxSrc[] =
            "#version 300 es\n"
            "in layout(location = 0) vec2 coord;\n"
            "out vec2 texCoord;\n"
            "void main() {\n"
            "  gl_Position = vec4((coord * 2.0) - 1.0, 0.0, 1.0);\n"
            // Note: Argus frames use a top-left origin and need to be inverted for GL texture use.
            "  texCoord = vec2(coord.x, 1.0 - coord.y);\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "#extension GL_OES_EGL_image_external : require\n"
            "precision highp float;\n"
            "uniform samplerExternalOES texSampler;\n"
            "in vec2 texCoord;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  fragColor = texture2D(texSampler, texCoord);\n"
            "}\n";
        PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_textureProgram));
        glUniform1i(glGetUniformLocation(m_textureProgram, "texSampler"), 0);
        glEnableVertexAttribArray(0);
    }

    // Create the shader program to render the face detection rectangles. It uses vertex
    // attrib 1 for the coordinates which are changed per-frame. The confidence (color
    // intensity) is also set per-frame.
    {
        static const char vtxSrc[] =
            "#version 300 es\n"
            "in layout(location = 1) vec2 coord;\n"
            "void main() {\n"
            "  gl_Position = vec4(coord.x * 2.0 - 1.0, coord.y * -2.0 + 1.0, 0.0, 1.0);\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "precision highp float;\n"
            "uniform vec3 color;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  fragColor = vec4(color, 1.0);\n"
            "}\n";
        PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_faceRectProgram));
        glUniform1i(glGetUniformLocation(m_textureProgram, "texSampler"), 0);
        glEnableVertexAttribArray(1);
        glLineWidth(4.0f);
    }

    // Create an external texture and connect it to the stream as a the consumer.
    CONSUMER_PRINT("Connecting to stream.\n");
    glGenTextures(1, &m_streamTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_streamTexture);
    if (!eglStreamConsumerGLTextureExternalKHR(g_display.get(), m_stream))
        ORIGINATE_ERROR("Unable to connect GL as consumer");
    CONSUMER_PRINT("Connected to stream.\n");

    // Set the acquire timeout to infinite.
    eglStreamAttribKHR(g_display.get(), m_stream, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1);

    return true;
}

bool ConsumerThread::threadExecute()
{
    // Wait until the Argus producer is connected.
    CONSUMER_PRINT("Waiting until producer is connected...\n");
    while (true)
    {
        EGLint state = EGL_STREAM_STATE_CONNECTING_KHR;
        if (!eglQueryStreamKHR(g_display.get(), m_stream, EGL_STREAM_STATE_KHR, &state))
            ORIGINATE_ERROR("Failed to query stream state (possible producer failure).");
        if (state != EGL_STREAM_STATE_CONNECTING_KHR)
            break;
        usleep(1000);
    }
    CONSUMER_PRINT("Producer is connected; continuing.\n");

    // Set the vertex attrib pointers/initial coords.
    static const GLfloat quadCoords[] = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    GLfloat faceRectCoords[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quadCoords);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, faceRectCoords);

    // Render until there are no more frames (the producer has disconnected).
    uint32_t frame = 0;
    while (eglStreamConsumerAcquireKHR(g_display.get(), m_stream))
    {
        frame++;
        CONSUMER_PRINT("Acquired frame %d. Rendering.\n", frame);

        // Render the image.
        glUseProgram(m_textureProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Get the metadata from the current EGLStream frame.
        // Note: This will likely fail for the last frame since the producer has
        //       already disconected from the EGLStream, so we need to handle
        //       failure gracefully.
        UniqueObj<EGLStream::MetadataContainer> metadataContainer(
            EGLStream::MetadataContainer::create(g_display.get(), m_stream));
        EGLStream::IArgusCaptureMetadata *iArgusCaptureMetadata =
            interface_cast<EGLStream::IArgusCaptureMetadata>(metadataContainer);
        if (iArgusCaptureMetadata)
        {
            CaptureMetadata *argusCaptureMetadata = iArgusCaptureMetadata->getMetadata();
            const Ext::IFaceDetectMetadata *iFaceDetectMetadata =
                interface_cast<const Ext::IFaceDetectMetadata>(argusCaptureMetadata);
            if (iFaceDetectMetadata)
            {
                std::vector<InterfaceProvider*> results;
                iFaceDetectMetadata->getFaceDetectResults(&results);

                // Colors to use for rectangles/text.
                static const float colors[] = {
                    1.0f, 0.1f, 0.0f,
                    0.0f, 1.0f, 0.5f,
                    0.3f, 0.0f, 1.0f,
                    1.0f, 0.8f, 0.0f,
                    0.2f, 0.7f, 1.0f,
                    1.0f, 0.4f, 0.7f
                };

                // Render rectangles around each face.
                for (uint32_t i = 0; i < results.size(); i++)
                {
                    const Ext::IFaceDetectResult* iFDR =
                        interface_cast<const Ext::IFaceDetectResult>(results[i]);
                    if (!iFDR)
                        ORIGINATE_ERROR("Failed to get IFaceDetectResults");
                    const NormalizedRect& rect = iFDR->getRect();

                    // Top left
                    faceRectCoords[0] = rect.left;
                    faceRectCoords[1] = rect.top;
                    // Bottom left
                    faceRectCoords[2] = rect.left;
                    faceRectCoords[3] = rect.bottom;
                    // Bottom right
                    faceRectCoords[4] = rect.right;
                    faceRectCoords[5] = rect.bottom;
                    // Top right
                    faceRectCoords[6] = rect.right;
                    faceRectCoords[7] = rect.top;

                    glUseProgram(m_faceRectProgram);
                    glUniform3f(glGetUniformLocation(m_faceRectProgram, "color"),
                                colors[(i%6)*3], colors[(i%6)*3+1], colors[(i%6)*3+2]);
                    glDrawArrays(GL_LINE_LOOP, 0, 4);
                }

                // Set the initial text rendering state.
                m_context.setTextPosition(0.02f, 0.98f);
                m_context.setTextBackground(0.2f, 0.2f, 0.2f, 1.0f);

                // Print the number of faces detected.
                char str[128];
                snprintf(str, sizeof(str), "Faces detected: %u\n", (uint32_t)results.size());
                m_context.setTextColor(1.0f, 1.0f, 1.0f);
                m_context.renderText(str);

                // Print rect/confidence values of each face.
                for (uint32_t i = 0; i < results.size(); i++)
                {
                    const Ext::IFaceDetectResult* iFDR =
                        interface_cast<const Ext::IFaceDetectResult>(results[i]);
                    if (!iFDR)
                        ORIGINATE_ERROR("Failed to get IFaceDetectResults");
                    const NormalizedRect& rect = iFDR->getRect();

                    snprintf(str, sizeof(str), " %d: r = [%0.2f, %0.2f, %0.2f, %0.2f], c = %.1f\n",
                             i, rect.left, rect.top, rect.right, rect.bottom,
                             iFDR->getConfidence());
                    m_context.setTextColor(colors[(i%6)*3], colors[(i%6)*3+1], colors[(i%6)*3+2]);
                    m_context.renderText(str);
                }
            }
        }

        PROPAGATE_ERROR(m_context.swapBuffers());
    }
    CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool ConsumerThread::threadShutdown()
{
    glDeleteProgram(m_textureProgram);
    glDeleteProgram(m_faceRectProgram);
    glDeleteTextures(1, &m_streamTexture);
    m_context.cleanup();

    CONSUMER_PRINT("Done.\n");

    return true;
}

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates an OutputStream to be consumed by
 *   the GL consumer, then performs repeating capture requests for CAPTURE_TIME
 *   seconds before closing the producer and Argus driver.
 ******************************************************************************/
static bool execute()
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(g_windowRect.left, g_windowRect.top,
                         g_windowRect.width, g_windowRect.height);
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    g_cameraProvider = UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(g_cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");

    // Ensure face detection is supported.
    if (!iCameraProvider->supportsExtension(EXT_FACE_DETECT))
        ORIGINATE_ERROR("Face detection not supported.");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    // Create the capture session using the first device and get its interfaces.
    PRODUCER_PRINT("Using camera %u of %u\n", g_cameraIndex, (unsigned)cameraDevices.size());
    UniqueObj<CaptureSession> captureSession(
            iCameraProvider->createCaptureSession(cameraDevices[g_cameraIndex]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    IEventProvider *iEventProvider = interface_cast<IEventProvider>(captureSession);
    Ext::IFaceDetectCaps *iFaceDetectCaps = interface_cast<Ext::IFaceDetectCaps>(captureSession);
    if (!iCaptureSession || !iEventProvider || !iFaceDetectCaps)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Print out the capture session's face detect limits.
    PRODUCER_PRINT("Maximum face detection results: %u\n",
                   iFaceDetectCaps->getMaxFaceDetectResults());

    // Create the OutputStream.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(iCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings = interface_cast<IOutputStreamSettings>(streamSettings);
    if (iStreamSettings)
    {
        iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iStreamSettings->setResolution(STREAM_SIZE);
        iStreamSettings->setEGLDisplay(g_display.get());
    }
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IStream *iStream = interface_cast<IStream>(outputStream);
    if (!iStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    // Launch the GL consumer thread to consume frames from the OutputStream's EGLStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    ConsumerThread glConsumerThread(iStream->getEGLStream());
    PROPAGATE_ERROR(glConsumerThread.initialize());

    // Wait until the consumer is connected to the stream.
    PROPAGATE_ERROR(glConsumerThread.waitRunning());

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());

    // Enable face detection.
    Ext::IFaceDetectSettings *iFDSettings = interface_cast<Ext::IFaceDetectSettings>(request);
    if (!iFDSettings)
        ORIGINATE_ERROR("Failed to get IFaceDetectSettings interface");
    iFDSettings->setFaceDetectEnable(true);

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for specified number of seconds.
    PROPAGATE_ERROR(window.pollingSleep(CAPTURE_TIME));

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the output stream. This destroys the EGLStream which causes
    // the GL consumer acquire to fail and the consumer thread to end.
    outputStream.reset();

    // Wait for the consumer thread to complete.
    PROPAGATE_ERROR(glConsumerThread.shutdown());

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
    if (argc == 6)
    {
        g_cameraIndex = atoi(argv[1]);
        g_windowRect.left = atoi(argv[2]);
        g_windowRect.top = atoi(argv[3]);
        g_windowRect.width = atoi(argv[4]);
        g_windowRect.height = atoi(argv[5]);
    }

    if (!ArgusSamples::execute())
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
