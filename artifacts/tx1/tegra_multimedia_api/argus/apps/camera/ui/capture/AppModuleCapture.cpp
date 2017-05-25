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

#include "AppModuleCapture.h"

#include <stdlib.h>

#include "Dispatcher.h"
#include "Error.h"
#include "Options.h"
#include "tasks/StillCapture.h"
#include "ScopedGuard.h"

namespace ArgusSamples
{

/* static */ bool AppModuleCapture::still(void *userPtr, const char *optArg)
{
    AppModuleCapture *module = reinterpret_cast<AppModuleCapture*>(userPtr);
    uint32_t count = 1;

    if (optArg)
    {
        count = atoi(optArg);
        if (count < 1)
            ORIGINATE_ERROR("'COUNT' is invalid, must be at least 1");
    }

    // start the capture module
    PROPAGATE_ERROR(module->start());
    // the scoped guard is used to call the stop function if following calls fail so that
    // the function is exited with module stopped.
    ScopedGuard<AppModuleCapture> runningGuard(module, &AppModuleCapture::stop);

    while (count)
    {
        PROPAGATE_ERROR(module->m_stillCapture.execute());
        PROPAGATE_ERROR(Window::getInstance().pollEvents());
        --count;
    }

    // stop the module
    runningGuard.cancel();
    PROPAGATE_ERROR(module->stop());

    return true;
}

/* static */ bool AppModuleCapture::capture(void *userPtr, const char *optArg)
{
    AppModuleCapture *module = reinterpret_cast<AppModuleCapture*>(userPtr);

    PROPAGATE_ERROR(module->m_stillCapture.execute());

    return true;
}

AppModuleCapture::AppModuleCapture()
    : m_initialized(false)
    , m_running(false)
    , m_guiContainerConfig(NULL)
    , m_captureButton(NULL)
{
}

AppModuleCapture::~AppModuleCapture()
{
    shutdown();
}

bool AppModuleCapture::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(m_stillCapture.initialize());

    PROPAGATE_ERROR(options.addOption(
        Options::Option("still", 's', "COUNT",
            Options::Option::TYPE_ACTION, Options::Option::FLAG_OPTIONAL_ARGUMENT,
            "do COUNT still captures and save as jpg files. If COUNT is not specified do one still "
            "capture.", still, this)));

    m_initialized = true;

    return true;
}

bool AppModuleCapture::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    PROPAGATE_ERROR_CONTINUE(m_stillCapture.shutdown());

    delete m_captureButton;
    m_captureButton = NULL;

    m_guiContainerConfig = NULL;

    m_initialized = false;
    return true;
}

bool AppModuleCapture::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    if (m_running)
        return true;

    // register key observer
    PROPAGATE_ERROR(Window::getInstance().registerObserver(this));

    // initialize the GUI
    if (iGuiContainerConfig && !m_guiContainerConfig)
    {
        PROPAGATE_ERROR(Window::IGuiElement::createAction("Capture",
            AppModuleCapture::capture, this, Window::IGuiElement::FLAG_NONE,
            Window::IGuiElement::ICON_NONE, &m_captureButton));

        m_guiContainerConfig = iGuiContainerConfig;
    }

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->add(m_captureButton));

    PROPAGATE_ERROR(m_stillCapture.start());

    m_running = true;

    return true;
}

bool AppModuleCapture::stop()
{
    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_stillCapture.stop());

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->remove(m_captureButton));

    // unregister key observer
    PROPAGATE_ERROR(Window::getInstance().unregisterObserver(this));

    m_running = false;

    return true;
}

bool AppModuleCapture::onKey(const Key &key)
{
    if (key == Key("space"))
    {
        PROPAGATE_ERROR(m_stillCapture.execute());
    }

    return true;
}

}; // namespace ArgusSamples
