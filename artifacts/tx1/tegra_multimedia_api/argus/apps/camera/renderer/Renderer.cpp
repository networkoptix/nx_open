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
#include "Renderer.h"
#include "StreamConsumer.h"
#include "Window.h"
#include "InitOnce.h"

namespace ArgusSamples
{

Renderer::Renderer()
    : m_initialized(false)
{
}

Renderer::~Renderer()
{
    if (!shutdown())
        REPORT_ERROR("Failed to shutdown renderer");
}

Renderer &Renderer::getInstance()
{
    static InitOnce initOnce;
    static Renderer instance;

    if (initOnce.begin())
    {
        if (instance.initialize())
        {
            initOnce.complete();
        }
        else
        {
            initOnce.failed();
            REPORT_ERROR("Initalization failed");
        }
    }

    return instance;
}

bool Renderer::initialize()
{
    if (m_initialized)
        return true;

    Window &window = Window::getInstance();

    PROPAGATE_ERROR(m_display.initialize(window.getEGLNativeDisplay()));

    m_initialized = true;

    return true;
}

bool Renderer::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(m_composer.shutdown());
    PROPAGATE_ERROR_CONTINUE(m_display.cleanup());

    m_initialized = false;

    return true;
}

bool Renderer::bindStream(EGLStreamKHR eglStream)
{
    PROPAGATE_ERROR(m_composer.bindStream(eglStream));
    return true;
}

bool Renderer::unbindStream(EGLStreamKHR eglStream)
{
    PROPAGATE_ERROR(m_composer.unbindStream(eglStream));
    return true;
}

bool Renderer::setStreamActive(EGLStreamKHR eglStream, bool active)
{
    PROPAGATE_ERROR(m_composer.setStreamActive(eglStream, active));
    return true;
}

bool Renderer::setStreamAspectRatio(EGLStreamKHR eglStream, float aspectRatio)
{
    PROPAGATE_ERROR(m_composer.setStreamAspectRatio(eglStream, aspectRatio));
    return true;
}

bool Renderer::reCompose()
{
    PROPAGATE_ERROR(m_composer.reCompose());
    return true;
}

}; // namespace ArgusSamples
