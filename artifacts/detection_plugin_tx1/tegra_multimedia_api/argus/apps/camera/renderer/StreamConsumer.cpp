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

#include <unistd.h>

#include "StreamConsumer.h"
#include "Renderer.h"
#include "Thread.h"
#include "Error.h"
#include "Util.h" // for TimeValue

namespace ArgusSamples
{

StreamConsumer::StreamConsumer(EGLStreamKHR eglStream)
    : m_eglStream(eglStream)
    , m_streamState(EGL_NONE)
    , m_streamTexture(0)
    , m_aspectRatio(1.0f)
#ifdef WAR_EGL_STREAM_RACE
    , m_sync(EGL_NO_SYNC_KHR)
#endif // WAR_EGL_STREAM_RACE
{
}

StreamConsumer::~StreamConsumer()
{
    shutdown();
}

bool StreamConsumer::initialize()
{
#ifdef WAR_EGL_STREAM_RACE
    PROPAGATE_ERROR(m_mutex.initialize());
#endif // WAR_EGL_STREAM_RACE
    PROPAGATE_ERROR(Thread::initialize());
    PROPAGATE_ERROR(Thread::waitRunning());
    return true;
}

bool StreamConsumer::shutdown()
{
    PROPAGATE_ERROR(Thread::shutdown());
    return true;
}

bool StreamConsumer::isEGLStream(EGLStreamKHR eglStream) const
{
    return (m_eglStream == eglStream);
}

uint32_t StreamConsumer::getStreamTextureID() const
{
    return m_streamTexture;
}

bool StreamConsumer::setStreamAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    return true;
}

float StreamConsumer::getStreamAspectRatio() const
{
    return m_aspectRatio;
}

#ifdef WAR_EGL_STREAM_RACE
Mutex& StreamConsumer::getStreamTextureMutex()
{
    return m_mutex;
}
#endif // WAR_EGL_STREAM_RACE

bool StreamConsumer::threadInitialize()
{
    Renderer &renderer = Renderer::getInstance();

    // Initialize the GL context and make it current.
    PROPAGATE_ERROR(m_context.initialize(&renderer.getComposerContext()));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Create an external texture and connect it to the stream as a the consumer.
    glGenTextures(1, &m_streamTexture);
    if (m_streamTexture == 0)
        ORIGINATE_ERROR("Failed to create GL texture");

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_streamTexture);
    if (!eglStreamConsumerGLTextureExternalKHR(renderer.getEGLDisplay(), m_eglStream))
        ORIGINATE_ERROR("Unable to connect GL as consumer");
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

#if defined(WAR_EGL_STREAM_RACE) && defined(EGL_NV_stream_sync)
    // create a sync object
    m_sync = eglCreateStreamSyncNV(renderer.getEGLDisplay(), m_eglStream, EGL_SYNC_NEW_FRAME_NV,
        NULL);
    if (m_sync == EGL_NO_SYNC_KHR)
        ORIGINATE_ERROR("eglCreateStreamSyncNV failed (error 0x%04x)", eglGetError());
#endif // WAR_EGL_STREAM_RACE && EGL_NV_stream_sync

    // Set a negative timeout for the stream acquire so that it blocks indefinitely.
    if (!eglStreamAttribKHR(renderer.getEGLDisplay(), m_eglStream,
        EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1))
    {
        ORIGINATE_ERROR("eglStreamAttribKHR failed (error 0x%04x)", eglGetError());
    }

    return true;
}

bool StreamConsumer::threadExecute()
{
    Renderer &renderer = Renderer::getInstance();
    const EGLDisplay display = renderer.getEGLDisplay();

    // check the stream state
    if ((m_streamState == EGL_NONE) ||
        (m_streamState == EGL_STREAM_STATE_CONNECTING_KHR)
#ifdef WAR_EGL_STREAM_RACE
        || !m_sync
#endif // WAR_EGL_STREAM_RACE
        )
    {
        if (!eglQueryStreamKHR(display, m_eglStream, EGL_STREAM_STATE_KHR, &m_streamState))
            ORIGINATE_ERROR("eglQueryStreamKHR failed");

        if (m_streamState == EGL_STREAM_STATE_CONNECTING_KHR)
        {
            // if the producer is not yet connected sleep and try again
            usleep(1000);
        }
    }

    if ((m_streamState == EGL_BAD_STREAM_KHR) ||
        (m_streamState == EGL_BAD_STATE_KHR))
    {
        ORIGINATE_ERROR("EGL stream is in bad state (0x%04x)", m_streamState);
    }
    else if ((m_streamState == EGL_STREAM_STATE_EMPTY_KHR) ||
             (m_streamState == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR) ||
             (m_streamState == EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR))
    {
#ifdef WAR_EGL_STREAM_RACE
        bool hasNewFrame = false;

        if (m_sync)
        {
            // wait for the sync object, use a timeout so that the thread can be shutdown if the
            // sync is never signaled.
            const EGLint ret = eglClientWaitSyncKHR(display, m_sync, 0 /*flags*/,
                TimeValue::fromMSec(1).toNSec());
            if (ret == EGL_CONDITION_SATISFIED_KHR)
            {
                // reset the sync object to unsignaled
                if (!eglSignalSyncKHR(display, m_sync, EGL_UNSIGNALED_KHR))
                    ORIGINATE_ERROR("eglSignalSyncKHR failed (error 0x%04x)", eglGetError());

                hasNewFrame = true;
            }
            else if (ret != EGL_TIMEOUT_EXPIRED_KHR)
            {
                ORIGINATE_ERROR("eglClientWaitSyncKHR failed with 0x%04x (error 0x%04x)", ret,
                    eglGetError());
            }
        }
        else
        {
            if (m_streamState == EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR)
            {
                hasNewFrame = true;
            }
            else
            {
                // no new frame, sleep for a while
                usleep(1000);
            }
        }

        if (hasNewFrame)
        {
            bool acquired = false;

            {
                // hold the mutex while acquiring to avoid a race with the composer thread
                ScopedMutex sm(m_mutex);
                PROPAGATE_ERROR(sm.expectLocked());

                acquired = eglStreamConsumerAcquireKHR(display, m_eglStream);
            }

            if (acquired)
            {
                // request a recompose from the renderer with the new image
                PROPAGATE_ERROR(renderer.reCompose());
            }
            else
            {
                // if the stream had been disconnected request a thread shutdown
                if (eglGetError() == EGL_BAD_STATE_KHR)
                    PROPAGATE_ERROR(requestShutdown());
                else
                    ORIGINATE_ERROR("Failed to aquire from egl stream");
            }
        }
#else // WAR_EGL_STREAM_RACE
        if (eglStreamConsumerAcquireKHR(display, m_eglStream))
        {
            // request a recompose from the renderer with the new image
            PROPAGATE_ERROR(renderer.reCompose());
        }
        else
        {
            // if the stream had been disconnected request a thread shutdown
            if (eglGetError() == EGL_BAD_STATE_KHR)
                PROPAGATE_ERROR(requestShutdown());
            else
                ORIGINATE_ERROR("Failed to aquire from egl stream");
        }
#endif // WAR_EGL_STREAM_RACE
    }

    return true;
}

bool StreamConsumer::threadShutdown()
{
#if defined(WAR_EGL_STREAM_RACE) && defined(EGL_NV_stream_sync)
    if (m_sync)
    {
        if (!eglDestroySyncKHR(Renderer::getInstance().getEGLDisplay(), m_sync))
            REPORT_ERROR("eglDestroySyncKHR failed (error 0x%04x)", eglGetError());
        m_sync = 0;
    }
#endif // WAR_EGL_STREAM_RACE && EGL_NV_stream_sync

    glDeleteTextures(1, &m_streamTexture);
    m_streamTexture = 0;

    PROPAGATE_ERROR(m_context.cleanup());

    return true;
}

}; // namespace ArgusSamples
