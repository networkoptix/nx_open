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

#ifndef STREAM_CONSUMER_H
#define STREAM_CONSUMER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "Thread.h"
#include "GLContext.h"

// Workaround for Bug 1736040, Jira CA-167
// Root cause is that the StreamConsumer updates the stream texture with eglStreamConsumerAcquire
// while the Composer is using the same texture to render. Although the EGL spec says that
// textures are 'latched', e.g. updated atomically occasionally the texture is undefined (content
// is zero, rendered as green in YUV color space) occasionally.
// The workaround uses a mutex to make sure that eglStreamConsumerAcquire is called only when the
// Composer is *not* using the texture. The StreamConsumer polls the EGL stream and takes the
// mutex if a new frame is available. It latches that frame and releases the mutex. The Composer
// holds the mutex while using the stream texture to render.
#define WAR_EGL_STREAM_RACE

#ifdef WAR_EGL_STREAM_RACE
#include "Mutex.h"
#endif // WAR_EGL_STREAM_RACE

namespace ArgusSamples
{

/**
 * This class handles creation of a thread acquiring from an EGL stream
 */
class StreamConsumer : public Thread
{
public:
    explicit StreamConsumer(EGLStreamKHR eglStream);
    ~StreamConsumer();

    bool initialize();
    bool shutdown();

    bool isEGLStream(EGLStreamKHR eglStream) const;
    uint32_t getStreamTextureID() const;

    bool setStreamAspectRatio(float aspectRatio);
    float getStreamAspectRatio() const;

#ifdef WAR_EGL_STREAM_RACE
    Mutex& getStreamTextureMutex();
#endif // WAR_EGL_STREAM_RACE

private:
    GLContext m_context;
    EGLStreamKHR m_eglStream;
    EGLint m_streamState;
    uint32_t m_streamTexture;
    float m_aspectRatio;        ///< aspect ration of the images transported by the stream
#ifdef WAR_EGL_STREAM_RACE
    EGLSyncKHR m_sync;          ///< stream sync object
    Mutex m_mutex;              ///< to protect access of the stream texture
#endif // WAR_EGL_STREAM_RACE

    /**
     * Hide default constructor
     */
    StreamConsumer();

    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
};

}; // namespace ArgusSamples

#endif // STREAM_CONSUMER_H
