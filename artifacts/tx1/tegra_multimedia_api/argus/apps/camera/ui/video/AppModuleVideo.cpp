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

#include "AppModuleVideo.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Dispatcher.h"
#include "Util.h"
#include "Error.h"
#include "Options.h"
#include "ScopedGuard.h"

namespace ArgusSamples
{

/* static */ bool AppModuleVideo::video(void *userPtr, const char *optArg)
{
    AppModuleVideo *module = reinterpret_cast<AppModuleVideo*>(userPtr);

    const float seconds = atof(optArg);
    if (seconds <= 0.0f)
        ORIGINATE_ERROR("'SECONDS' is invalid, must not be less than or equal to zero");

    // start the video module
    PROPAGATE_ERROR(module->start());
    // the scoped guard is used to call the stop function if following calls fail so that
    // the function is exited with module stopped.
    ScopedGuard<AppModuleVideo> runningGuard(module, &AppModuleVideo::stop);

    const TimeValue endTime = getCurrentTime() + TimeValue::fromSec(seconds);

    // start the recording
    PROPAGATE_ERROR(module->m_videoRecord.startRecording());
    ScopedGuard<TaskVideoRecord> recordingGuard(&module->m_videoRecord,
        &TaskVideoRecord::stopRecording);

    // wait until the time has elapsed
    while (getCurrentTime() < endTime)
    {
        PROPAGATE_ERROR(Window::getInstance().pollEvents());
        usleep(1000);
    }

    // stop recording
    recordingGuard.cancel();
    PROPAGATE_ERROR(module->m_videoRecord.stopRecording());

    // stop the module
    runningGuard.cancel();
    PROPAGATE_ERROR(module->stop());

    return true;
}

/* static */ bool AppModuleVideo::toggleRecording(void *userPtr, const char *optArg)
{
    AppModuleVideo *module = reinterpret_cast<AppModuleVideo*>(userPtr);

    PROPAGATE_ERROR(module->m_videoRecord.toggleRecording());

    return true;
}

/* static */ bool AppModuleVideo::videoBitRate(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_videoBitRate.setFromString(optArg));

    return true;
}

/* static */ bool AppModuleVideo::videoFormat(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_videoFormat.setFromString(optArg));
    return true;
}

/* static */ bool AppModuleVideo::videoFileType(void *userPtr, const char *optArg)
{
    PROPAGATE_ERROR(Dispatcher::getInstance().m_videoFileType.setFromString(optArg));
    return true;
}

AppModuleVideo::AppModuleVideo()
    : m_initialized(false)
    , m_running(false)
    , m_guiContainerConfig(NULL)
    , m_guiConfig(NULL)
{
}

AppModuleVideo::~AppModuleVideo()
{
    shutdown();
}

bool AppModuleVideo::initialize(Options &options)
{
    if (m_initialized)
        return true;

    PROPAGATE_ERROR(m_videoRecord.initialize());

    PROPAGATE_ERROR(options.addOption(
        Options::Option("video", 'v', "DURATION",
            Options::Option::TYPE_ACTION, Options::Option::FLAG_REQUIRED_ARGUMENT,
            "record video for DURATION seconds and save to a file.", video, this)));

    PROPAGATE_ERROR(options.addOption(
        Options::Option("videobitrate", 0, "RATE", Dispatcher::getInstance().m_videoBitRate,
            "set the video bit rate mode to RATE. If RATE is zero a reasonable default "
            "is selected.", videoBitRate)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("videoformat", 0, "FORMAT", Dispatcher::getInstance().m_videoFormat,
            "set the video format.", videoFormat)));
    PROPAGATE_ERROR(options.addOption(
        Options::Option("videofiletype", 0, "TYPE", Dispatcher::getInstance().m_videoFileType,
            "set the video file type. For 'h265' set the file type as 'mkv' since 'h265' is only "
            "supported by the 'mkv' container.", videoFileType)));

    m_initialized = true;

    return true;
}

bool AppModuleVideo::shutdown()
{
    if (!m_initialized)
        return true;

    PROPAGATE_ERROR_CONTINUE(stop());

    PROPAGATE_ERROR_CONTINUE(m_videoRecord.shutdown());

    delete m_guiConfig;
    m_guiConfig = NULL;

    m_guiContainerConfig = NULL;

    m_initialized = false;

    return true;
}

bool AppModuleVideo::start(Window::IGuiMenuBar *iGuiMenuBar,
    Window::IGuiContainer *iGuiContainerConfig)
{
    if (m_running)
        return true;

    // register key observer
    PROPAGATE_ERROR(Window::getInstance().registerObserver(this));

    // initialize the GUI
    if (iGuiContainerConfig && !m_guiConfig)
    {
        // initialize the GUI

        // create a grid container
        PROPAGATE_ERROR(Window::IGuiContainerGrid::create(&m_guiConfig));

        // create the elements
        UniquePointer<Window::IGuiElement> element;
        Dispatcher &dispatcher = Dispatcher::getInstance();

        Window::IGuiContainerGrid::BuildHelper buildHelper(m_guiConfig);

#define CREATE_GUI_ELEMENT(_NAME, _VALUE)                                           \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(&dispatcher._VALUE, &element));\
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

#define CREATE_GUI_ELEMENT_COMBO_BOX(_NAME, _VALUE, _FROMTYPE, _TOTYPE)             \
        assert(sizeof(_FROMTYPE) == sizeof(_TOTYPE));                               \
        PROPAGATE_ERROR(Window::IGuiElement::createValue(reinterpret_cast<          \
            Value<_TOTYPE>*>(&dispatcher._VALUE), &element));                       \
        PROPAGATE_ERROR(buildHelper.append(_NAME, element.get()));                  \
        element.release();

        CREATE_GUI_ELEMENT_COMBO_BOX("Video Format", m_videoFormat,
            VideoPipeline::VideoFormat, Window::IGuiElement::ValueTypeEnum);
        CREATE_GUI_ELEMENT_COMBO_BOX("Video File Type", m_videoFileType,
            VideoPipeline::VideoFileType, Window::IGuiElement::ValueTypeEnum);

        CREATE_GUI_ELEMENT("Video Bit Rate", m_videoBitRate);

#undef CREATE_GUI_ELEMENT
#undef CREATE_GUI_ELEMENT_COMBO_BOX

        PROPAGATE_ERROR(Window::IGuiElement::createAction("Toggle Recording",
            AppModuleVideo::toggleRecording, this, Window::IGuiElement::FLAG_BUTTON_TOGGLE,
            Window::IGuiElement::ICON_MEDIA_RECORD, &element));
        PROPAGATE_ERROR(buildHelper.append(element.get(), 2));
        element.release();

        m_guiContainerConfig = iGuiContainerConfig;
    }

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->add(m_guiConfig));

    PROPAGATE_ERROR(m_videoRecord.start());

    m_running = true;

    return true;
}

bool AppModuleVideo::stop()
{
    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_videoRecord.stop());

    if (m_guiContainerConfig)
        PROPAGATE_ERROR(m_guiContainerConfig->remove(m_guiConfig));

    // unregister key observer
    PROPAGATE_ERROR(Window::getInstance().unregisterObserver(this));

    m_running = false;

    return true;
}

bool AppModuleVideo::onKey(const Key &key)
{
    if (key == Key("space"))
    {
        PROPAGATE_ERROR(m_videoRecord.toggleRecording());
    }

    return true;
}

}; // namespace ArgusSamples
