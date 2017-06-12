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

#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

#include <math.h>

#include "Error.h"
#include "UniquePointer.h"

#include "Composer.h"
#include "Renderer.h"
#include "Window.h"
#include "StreamConsumer.h"

namespace ArgusSamples
{

Composer::Composer()
    : m_initialized(false)
    , m_program(0)
    , m_windowWidth(0)
    , m_windowHeight(0)
    , m_windowAspectRatio(1.0f)
    , m_clientSequence(0)
    , m_composerSequence(0)
{
}

Composer::~Composer()
{
    shutdown();
}

bool Composer::initialize()
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(m_mutex.initialize());

    PROPAGATE_ERROR(m_sequenceMutex.initialize());
    PROPAGATE_ERROR(m_sequenceCond.initialize());

    // initialize the window size
    Window &window = Window::getInstance();
    PROPAGATE_ERROR(onResize(window.getWidth(), window.getHeight()));

    // and register as observer for size changes
    PROPAGATE_ERROR(window.registerObserver(this));

    PROPAGATE_ERROR(Thread::initialize());
    PROPAGATE_ERROR(Thread::waitRunning());

    m_initialized = true;

    return true;
}

bool Composer::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(Window::getInstance().unregisterObserver(this));

    // request shutdown of the thread
    PROPAGATE_ERROR_CONTINUE(Thread::requestShutdown());

    // trigger a re-compose to wake up the thread
    PROPAGATE_ERROR_CONTINUE(reCompose());

    PROPAGATE_ERROR_CONTINUE(Thread::shutdown());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
        PROPAGATE_ERROR_CONTINUE(it->m_consumer->shutdown());
        delete it->m_consumer;
    }
    m_streams.clear();

    m_initialized = false;

    return true;
}

bool Composer::bindStream(EGLStreamKHR eglStream)
{
    if (eglStream == EGL_NO_STREAM_KHR)
        ORIGINATE_ERROR("Invalid stream");

    PROPAGATE_ERROR(initialize());

    UniquePointer<StreamConsumer> streamConsumer(new StreamConsumer(eglStream));
    if (!streamConsumer)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(streamConsumer->initialize());

    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    m_streams.push_back(Stream(streamConsumer.release()));

    // trigger a re-compose because the stream configuration changed
    PROPAGATE_ERROR(reCompose());

    return true;
}

bool Composer::unbindStream(EGLStreamKHR eglStream)
{
    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
       if (it->m_consumer->isEGLStream(eglStream))
       {
            PROPAGATE_ERROR(it->m_consumer->shutdown());
            delete it->m_consumer;
            m_streams.erase(it);
            // trigger a re-compose because the stream configuration changed
            PROPAGATE_ERROR(reCompose());
            return true;
        }
    }

    ORIGINATE_ERROR("Stream was not bound");

    return true;
}

bool Composer::setStreamActive(EGLStreamKHR eglStream, bool active)
{
    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
       if (it->m_consumer->isEGLStream(eglStream))
       {
            it->m_active = active;
            // trigger a re-compose because the stream configuration changed
            PROPAGATE_ERROR(reCompose());
            return true;
        }
    }

    ORIGINATE_ERROR("Stream was not bound");

    return true;
}

bool Composer::setStreamAspectRatio(EGLStreamKHR eglStream, float aspectRatio)
{
    ScopedMutex sm(m_mutex);
    PROPAGATE_ERROR(sm.expectLocked());

    for (StreamList::iterator it = m_streams.begin(); it != m_streams.end(); ++it)
    {
       if (it->m_consumer->isEGLStream(eglStream))
       {
            PROPAGATE_ERROR(it->m_consumer->setStreamAspectRatio(aspectRatio));
            // trigger a re-compose because the stream configuration changed
            PROPAGATE_ERROR(reCompose());
            return true;
        }
    }

    ORIGINATE_ERROR("Stream was not bound");

    return true;
}

bool Composer::reCompose()
{
    ScopedMutex sm(m_sequenceMutex);
    PROPAGATE_ERROR(sm.expectLocked());

    ++m_clientSequence;
    PROPAGATE_ERROR(m_sequenceCond.signal());

    return true;
}

bool Composer::onResize(uint32_t width, uint32_t height)
{
    m_windowWidth = width;
    m_windowHeight = height;
    m_windowAspectRatio = (float)width / (float)height;
    return true;
}

bool Composer::threadInitialize()
{
    Renderer &renderer = Renderer::getInstance();

    // Initialize the GL context and make it current.
    PROPAGATE_ERROR(m_context.initialize(&Window::getInstance()));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Create the shader program.
    static const char vtxSrc[] =
        "#version 300 es\n"
        "#extension GL_ARB_explicit_uniform_location : require\n"
        "in layout(location = 0) vec2 vertex;\n"
        "out vec2 vTexCoord;\n"
        "layout(location = 0) uniform vec2 offset;\n"
        "layout(location = 1) uniform vec2 scale;\n"
        "void main() {\n"
        "  gl_Position = vec4((offset + vertex * scale) * 2.0 - 1.0, 0.0, 1.0);\n"
        "  vTexCoord = vec2(vertex.x, 1.0 - vertex.y);\n"
        "}\n";
    static const char frgSrc[] =
        "#version 300 es\n"
        "#extension GL_OES_EGL_image_external : require\n"
        "precision highp float;\n"
        "uniform samplerExternalOES texSampler;\n"
        "in vec2 vTexCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "  fragColor = texture2D(texSampler, vTexCoord);\n"
        "}\n";
    PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_program));

    // Setup vertex state.
    static const GLfloat vertices[] = {
         0.0f, 0.0f,
         0.0f, 1.0f,
         1.0f, 0.0f,
         1.0f, 1.0f,
    };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);

    if (eglSwapInterval(renderer.getEGLDisplay(), 1) != EGL_TRUE)
        ORIGINATE_ERROR("Failed to set the swap interval");

    return true;
}

bool Composer::threadExecute()
{
    {
        // wait for the sequence condition
        ScopedMutex sm(m_sequenceMutex);
        PROPAGATE_ERROR(sm.expectLocked());

        PROPAGATE_ERROR(m_sequenceCond.wait(m_sequenceMutex));

        // check if the sequence differs
        if (m_clientSequence == m_composerSequence)
            return true;

        // need to compose
        m_composerSequence = m_clientSequence;
    }

    glViewport(0,0, m_windowWidth, m_windowHeight);

    glClear(GL_COLOR_BUFFER_BIT);

    // take a copy of the stream list to avoid holding the mutex too long
    StreamList streamsCopy;
    {
        ScopedMutex sm(m_mutex);
        PROPAGATE_ERROR(sm.expectLocked());

        streamsCopy = m_streams;
    }

    if (streamsCopy.size() != 0)
    {
        // iterate through the streams and render them

        glUseProgram(m_program);

        uint32_t activeStreams = 0;
        for (StreamList::iterator it = streamsCopy.begin(); it != streamsCopy.end(); ++it)
        {
            if (it->m_active)
                ++activeStreams;
        }

        const uint32_t cells = static_cast<uint32_t>(ceil(sqrt(activeStreams)));
        const float scaleX = 1.0f / cells;
        const float scaleY = 1.0f / cells;
        uint32_t offsetX = 0, offsetY = cells - 1;

        for (StreamList::iterator it = streamsCopy.begin(); it != streamsCopy.end(); ++it)
        {
            if (it->m_active)
            {
#ifdef WAR_EGL_STREAM_RACE
                // hold the stream texture mutex while drawing with the texture
                ScopedMutex sm(it->m_consumer->getStreamTextureMutex());
                PROPAGATE_ERROR(sm.expectLocked());
#endif // WAR_EGL_STREAM_RACE

                glBindTexture(GL_TEXTURE_EXTERNAL_OES, it->m_consumer->getStreamTextureID());

                // scale according to aspect ratios
                float sizeX = it->m_consumer->getStreamAspectRatio() / m_windowAspectRatio;
                float sizeY = 1.0f;

                if (sizeX > sizeY)
                {
                    sizeY /= sizeX;
                    sizeX = 1.0f;
                }
                else
                {
                    sizeX /= sizeY;
                    sizeY = 1.0f;
                }

                glUniform2f(0,
                    (offsetX - (sizeX - 1.0f) * 0.5f) * scaleX,
                    (offsetY - (sizeY - 1.0f) * 0.5f) * scaleY);
                glUniform2f(1, scaleX * sizeX, scaleY * sizeY);

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

                ++offsetX;
                if (offsetX == cells)
                {
                    --offsetY;
                    offsetX = 0;
                }
            }
        }

        glUseProgram(0);
    }

    PROPAGATE_ERROR(m_context.swapBuffers());

    return true;
}

bool Composer::threadShutdown()
{
    glDeleteProgram(m_program);
    m_program = 0;

    PROPAGATE_ERROR(m_context.cleanup());

    return true;
}

}; // namespace ArgusSamples
