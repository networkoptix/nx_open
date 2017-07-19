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

#include "AppModuleMultiExposure.h"
#include "Options.h"
#include "Error.h"

namespace ArgusSamples
{

/* static */ bool AppModuleMultiExposure::exposureRange(void *userPtr, const char *optArg)
{
    AppModuleMultiExposure *module = reinterpret_cast<AppModuleMultiExposure*>(userPtr);
    PROPAGATE_ERROR(module->m_multiExposure.m_exposureRange.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleMultiExposure::exposureSteps(void *userPtr, const char *optArg)
{
    AppModuleMultiExposure *module = reinterpret_cast<AppModuleMultiExposure*>(userPtr);
    PROPAGATE_ERROR(module->m_multiExposure.m_exposureSteps.setFromString(optArg));
    return true;
}

AppModuleMultiExposure::AppModuleMultiExposure()
    : m_initialized(false)
    , m_running(false)
    , m_guiContainerConfig(NULL)
    , m_guiConfig(NULL)
{
}

AppModuleMultiExposure::~AppModuleMultiExposure()
{
    shutdown();
}

bool AppModuleMultiExposure::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(m_multiExposure.initialize());

    PROPAGATE_ERROR(options.addOption(
        Options::Option("exposurerange", 0, "RANGE", m_multiExposure.m_exposureRange,
            "set the exposure range to RANGE.", exposureRange, this)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("exposuresteps", 0, "COUNT", m_multiExposure.m_exposureSteps,
            "sample the exposure range at COUNT steps.",
            exposureSteps, this)));

    m_initialized = true;

    return true;
}

bool AppModuleMultiExposure::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    PROPAGATE_ERROR_CONTINUE(m_multiExposure.shutdown());

    delete m_guiConfig;
    m_guiConfig = NULL;

    m_guiContainerConfig = NULL;

    m_initialized = false;

    return true;
}

bool AppModuleMultiExposure::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    if (m_running)
        return true;

    // initialize the GUI
    if (iGuiContainerConfig && !m_guiConfig)
    {
        // initialize the GUI

        // create a grid container
        PROPAGATE_ERROR(Window::IGuiContainerGrid::create(&m_guiConfig));

        // create the elements
        UniquePointer<Window::IGuiElement> element;

        Window::IGuiContainerGrid::BuildHelper buildHelper(m_guiConfig);

#define CREATE_GUI_ELEMENT(_NAME, _VALUE)                                       \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(&_VALUE, &element));   \
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));              \
        element.release();

        CREATE_GUI_ELEMENT("Exposure Range", m_multiExposure.m_exposureRange);
        CREATE_GUI_ELEMENT("Exposure Steps", m_multiExposure.m_exposureSteps);

#undef CREATE_GUI_ELEMENT

        m_guiContainerConfig = iGuiContainerConfig;
    }

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->add(m_guiConfig));

    PROPAGATE_ERROR(m_multiExposure.start());

    m_running = true;

    return true;
}

bool AppModuleMultiExposure::stop()
{
    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_multiExposure.stop());

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->remove(m_guiConfig));

    m_running = false;

    return true;
}

}; // namespace ArgusSamples
