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

#include "EGLGlobal.h"
#include "Window.h"
#include "Error.h"
#include "InitOnce.h"

#include <string.h>

namespace ArgusSamples
{

// EGL extension functions.
#define EGL_EXTN_FUNC(_type, _name) _type _name = NULL;
EGL_EXTN_FUNC_LIST
#undef EGL_EXTN_FUNC

EGLDisplayHolder::EGLDisplayHolder() :
    m_display(EGL_NO_DISPLAY)
{
}

EGLDisplayHolder::~EGLDisplayHolder()
{
    cleanup();
}

bool EGLDisplayHolder::initialize(EGLNativeDisplayType native)
{
    // Get the default display
    m_display = eglGetDisplay(native);

#ifdef EGL_EXT_platform_device
    // If the default display isn't available, try to use the EXT_platform_device extension.
    if (m_display == EGL_NO_DISPLAY)
    {
        const char* eglClientExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        if (eglClientExtensions)
        {
            // Check for the required client extensions.
            if (!strstr(eglClientExtensions, "EGL_EXT_client_extensions") ||
                !strstr(eglClientExtensions, "EGL_EXT_device_base") ||
                !strstr(eglClientExtensions, "EGL_EXT_platform_base") ||
                !strstr(eglClientExtensions, "EGL_EXT_platform_device"))
            {
                ORIGINATE_ERROR("EXT_platform_device extension missing");
            }

            // Get the client extension functions.
#define EGL_EXTN(_type, _name) _type _name = (_type)eglGetProcAddress(#_name);
            EGL_EXTN(PFNEGLQUERYDEVICESEXTPROC, eglQueryDevicesEXT);
            EGL_EXTN(PFNEGLGETPLATFORMDISPLAYEXTPROC, eglGetPlatformDisplayEXT);
#undef EGL_EXTN

            // Find an EGL device (uses the first available).
            EGLDeviceEXT device;
            EGLint numDevices;
            if (!eglQueryDevicesEXT(1, &device, &numDevices) || numDevices != 1)
                ORIGINATE_ERROR("No EGL device available");

            // Get the display from the platform device.
            m_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, device, NULL);
        }
    }
#endif

    if (m_display == EGL_NO_DISPLAY)
        ORIGINATE_ERROR("Could not get EGL display");

    if (!eglInitialize(m_display, 0, 0))
        ORIGINATE_ERROR("Could not initialize EGL display");

    // Check for required extensions.
    const char* eglExtensions = eglQueryString(m_display, EGL_EXTENSIONS);
    if (!strstr(eglExtensions, "EGL_KHR_stream"))
        ORIGINATE_ERROR("EGL_KHR_stream not supported");
    if (!strstr(eglExtensions, "EGL_KHR_stream_consumer_gltexture"))
        ORIGINATE_ERROR("EGL_KHR_stream_consumer_gltexture not supported");
    if (!strstr(eglExtensions, "EGL_KHR_stream_producer_eglsurface"))
        ORIGINATE_ERROR("EGL_KHR_stream_producer_eglsurface not supported");
    if (!strstr(eglExtensions, "EGL_KHR_fence_sync"))
        ORIGINATE_ERROR("EGL_KHR_fence_sync not supported");
    if (!strstr(eglExtensions, "EGL_KHR_reusable_sync"))
        ORIGINATE_ERROR("EGL_KHR_reusable_sync not supported");
    // EGL_NV_stream_sync is optional

    // do the initialization only once
    static InitOnce initOnce;
    if (initOnce.begin())
    {
        // Define the macro to get extension functions.
#define EGL_EXTN_FUNC(_type, _name) \
        _name = (_type)eglGetProcAddress(#_name); \
        if (!_name) \
        { \
            initOnce.failed(); \
            ORIGINATE_ERROR("Failed to get function:" #_name "\n"); \
        }

        // Get the extension function pointers (uses EGL_EXTN_FUNC macro)
        EGL_EXTN_FUNC_LIST

#undef EGL_EXTN_FUNC

        initOnce.complete();
    }

    return true;
}

bool EGLDisplayHolder::cleanup()
{
    if (m_display != EGL_NO_DISPLAY)
    {
        // inform the window that the display is gone
        Window::getInstance().onDisplayTermination(m_display);

        // continue even on failure, just report it
        if (eglTerminate(m_display) != EGL_TRUE)
            REPORT_ERROR("eglTerminate failed (error 0x%04x)", eglGetError());
        m_display = EGL_NO_DISPLAY;
    }

    return true;
}

EGLDisplay EGLDisplayHolder::get() const
{
    return m_display;
}

EGLStreamHolder::EGLStreamHolder() :
    m_display(EGL_NO_DISPLAY),
    m_stream(EGL_NO_STREAM_KHR)
{
}

EGLStreamHolder::~EGLStreamHolder()
{
    destroy();
}

bool EGLStreamHolder::create(EGLDisplay display)
{
    if (m_stream != EGL_NO_STREAM_KHR)
        ORIGINATE_ERROR("Already created");

    m_display = display;
    m_stream = eglCreateStreamKHR(m_display, NULL);
    if (m_stream == EGL_NO_STREAM_KHR)
        ORIGINATE_ERROR("Unable to create stream");

    // The stream should be in the CREATED state.
    CHECK_STREAM_STATE(*this, CREATED);

    return true;
}

bool EGLStreamHolder::destroy()
{
    if (m_stream != EGL_NO_STREAM_KHR)
    {
        if (!eglDestroyStreamKHR(m_display, m_stream))
            ORIGINATE_ERROR("Error destroying EGLStream");
        m_stream = EGL_NO_STREAM_KHR;
    }

    return true;
}

EGLStreamKHR EGLStreamHolder::get() const
{
    return m_stream;
}

EGLint EGLStreamHolder::getState() const
{
    EGLint streamState;

    eglQueryStreamKHR(m_display, m_stream, EGL_STREAM_STATE_KHR, &streamState);

    return streamState;
}

} // namespace ArgusSamples
