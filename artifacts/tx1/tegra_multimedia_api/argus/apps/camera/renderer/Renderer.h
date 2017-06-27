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

#ifndef RENDERER_H
#define RENDERER_H

#include "Composer.h"
#include "EGLGlobal.h"

namespace ArgusSamples
{

/**
 * Renderer. Provide functions to create/destroy EGL streams and display them.
 */
class Renderer
{
public:
    /**
     * Get the window instance.
     */
    static Renderer &getInstance();

    /**
     * Shutdown, free all resources
     */
    bool shutdown();

    /**
     * Bind an EGL stream
     *
     * @param eglStream [in]
     */
    bool bindStream(EGLStreamKHR eglStream);

    /**
     * Unbind a bound EGL stream
     *
     * @param eglStream [in]
     */
    bool unbindStream(EGLStreamKHR eglStream);

    /**
     * Set the active state of the stream, only active streams are rendered
     *
     * @param eglStream [in]
     * @param active [in]
     */
    bool setStreamActive(EGLStreamKHR eglStream, bool active);

    /**
     * Set the stream aspect ratio
     *
     * @param eglStream [in]
     * @param aspectRatio [in] aspect ration of the images transported by the stream
     */
    bool setStreamAspectRatio(EGLStreamKHR eglStream, float aspectRatio);

    /**
     * Get the EGL display
     */
    EGLDisplay getEGLDisplay()
    {
        if (initialize())
            return m_display.get();

        return EGL_NO_DISPLAY;
    }

    /**
     * Get the composer EGL context
     */
    const GLContext& getComposerContext() const
    {
        return m_composer.getContext();
    }

    /**
     * Trigger a re-compose. Called when new images arrived in a stream.
     */
    bool reCompose();

private:
    Renderer();
    ~Renderer();

    // this is a singleton, hide copy constructor etc.
    Renderer(const Renderer&);
    Renderer& operator=(const Renderer&);

    bool initialize();

    bool m_initialized;         ///< set if initialized

    EGLDisplayHolder m_display; ///< EGL display

    Composer m_composer;        ///< the composer renders streams
};

}; // namespace ArgusSamples

#endif // RENDERER_H
