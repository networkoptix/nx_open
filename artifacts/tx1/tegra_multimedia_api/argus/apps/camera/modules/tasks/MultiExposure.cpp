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

#include <sstream>

#include "MultiExposure.h"
#include "Renderer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "UniquePointer.h"

namespace ArgusSamples
{

TaskMultiExposure::TaskMultiExposure()
    : m_exposureStepsRange(3)
    , m_exposureSteps(new ValidatorRange<uint32_t>(&m_exposureStepsRange), 3)
    , m_exposureRange(
        new ValidatorRange<Argus::Range<float> >(
            Argus::Range<float>(-10.0f, 10.0f),
            Argus::Range<float>(-10.0f, 10.0f)),
        Argus::Range<float>(-2.0f, 2.0f))
    , m_initialized(false)
    , m_running(false)
    , m_wasRunning(false)
    , m_captureIndex(0)
{
}

TaskMultiExposure::~TaskMultiExposure()
{
    shutdown();
}

TaskMultiExposure::ExpLevel::ExpLevel()
{
}

TaskMultiExposure::ExpLevel::~ExpLevel()
{
    shutdown();
}

bool TaskMultiExposure::ExpLevel::initialize(float exposureCompensation)
{
    Renderer &renderer = Renderer::getInstance();
    Dispatcher &dispatcher = Dispatcher::getInstance();

    // create the request
    PROPAGATE_ERROR(dispatcher.createRequest(m_request, Argus::CAPTURE_INTENT_STILL_CAPTURE));

    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(m_request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to get IRequest interface");

    // get the autocontrol settings and set the exposure compensation value
    Argus::IAutoControlSettings *iAutoControlSettings =
        Argus::interface_cast<Argus::IAutoControlSettings>(iRequest->getAutoControlSettings());
    if (!iAutoControlSettings)
        ORIGINATE_ERROR("Failed to get IAutoControlSettings interface");

    // lock AE
    if (iAutoControlSettings->setAeLock(true) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to set AE lock");

    // set exposure compensation value
    if (iAutoControlSettings->setExposureCompensation(exposureCompensation) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to set exposure compensation");

    // Create the preview stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_request.get(), m_outputStream));

    Argus::IStream *iStream = Argus::interface_cast<Argus::IStream>(m_outputStream.get());
    if (!iStream)
        ORIGINATE_ERROR("Failed to get IStream interface");

    // Bind the stream to the renderer
    PROPAGATE_ERROR(renderer.bindStream(iStream->getEGLStream()));

    const Argus::Size streamSize = iStream->getResolution();
    PROPAGATE_ERROR(renderer.setStreamAspectRatio(iStream->getEGLStream(),
        (float)streamSize.width / (float)streamSize.height));

    // Enable the output stream
    PROPAGATE_ERROR(dispatcher.enableOutputStream(m_request.get(), m_outputStream.get()));

    return true;
}

bool TaskMultiExposure::ExpLevel::shutdown()
{
    if (m_request)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        if (m_outputStream)
        {
            // disable the output stream
            PROPAGATE_ERROR_CONTINUE(dispatcher.disableOutputStream(m_request.get(),
                m_outputStream.get()));

            const EGLStreamKHR eglStream =
                Argus::interface_cast<Argus::IStream>(m_outputStream)->getEGLStream();

            m_outputStream.reset();

            // unbind the EGL stream from the renderer
            PROPAGATE_ERROR_CONTINUE(Renderer::getInstance().unbindStream(eglStream));
        }

        // destroy the request
        PROPAGATE_ERROR_CONTINUE(dispatcher.destroyRequest(m_request));
    }

    return true;
}

bool TaskMultiExposure::initialize()
{
    if (m_initialized)
        return true;

    Dispatcher &dispatcher = Dispatcher::getInstance();

    PROPAGATE_ERROR_CONTINUE(dispatcher.m_deviceOpen.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiExposure::onDeviceOpenChanged)));

    m_initialized = true;

    // register the device observers after 'm_initialize' is set, the call back will be
    // called immediately and assert that 'm_initialize' is set
    PROPAGATE_ERROR_CONTINUE(m_exposureRange.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiExposure::onParametersChanged)));
    PROPAGATE_ERROR_CONTINUE(m_exposureSteps.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiExposure::onParametersChanged)));

    return true;
}

bool TaskMultiExposure::shutdown()
{
    if (!m_initialized)
        return true;

    // stop the preview
    PROPAGATE_ERROR(stop());

    PROPAGATE_ERROR_CONTINUE(Dispatcher::getInstance().m_deviceOpen.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiExposure::onDeviceOpenChanged)));
    PROPAGATE_ERROR_CONTINUE(m_exposureRange.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiExposure::onParametersChanged)));
    PROPAGATE_ERROR_CONTINUE(m_exposureSteps.unregisterObserver(this,
        static_cast<IObserver::CallbackFunction>(&TaskMultiExposure::onParametersChanged)));

    m_initialized = false;

    return true;
}

bool TaskMultiExposure::shutdownExpLevels()
{
    if (!m_expLevels.empty())
    {
        // shutdown the exposure streams
        for (std::list<ExpLevel*>::iterator it = m_expLevels.begin(); it != m_expLevels.end(); ++it)
        {
            ExpLevel *expLevel = *it;
            PROPAGATE_ERROR_CONTINUE(expLevel->shutdown());
            delete expLevel;
        }
        m_expLevels.clear();
    }

    return true;
}

bool TaskMultiExposure::onDeviceOpenChanged(const Observed &source)
{
    const bool isOpen = static_cast<const Value<bool>&>(source).get();

    // If the current device is closed the request needs to be recreated on the new device. Stop
    // and then start when the device is opened again.
    if (!isOpen)
    {
        m_wasRunning = m_running;
        PROPAGATE_ERROR(stop());
    }
    else
    {
        uint32_t maxBurstRequests = Dispatcher::getInstance().maxBurstRequests();
        PROPAGATE_ERROR(m_exposureStepsRange.set(Argus::Range<uint32_t>(2, maxBurstRequests)));

        if (m_wasRunning)
        {
            m_wasRunning = false;
            PROPAGATE_ERROR(start());
        }
    }

    return true;
}

bool TaskMultiExposure::onParametersChanged(const Observed &source)
{
    assert(m_initialized);

    // if preview is running, stop, shutdown levels, set the new value and start again
    const bool wasRunning = m_running;
    PROPAGATE_ERROR(stop());
    PROPAGATE_ERROR(shutdownExpLevels());

    if (wasRunning)
        PROPAGATE_ERROR(start());

    return true;
}

bool TaskMultiExposure::start()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (m_running)
        return true;

    if (m_expLevels.empty())
    {
        std::ostringstream message;

        message << "Creating " << m_exposureSteps << " streams with exposure compensation set to ";

        // create a request and streams for each exposure level
        for (uint32_t requestIndex = 0; requestIndex < m_exposureSteps; ++requestIndex)
        {
            UniquePointer<ExpLevel> expLevel(new ExpLevel);

            if (!expLevel)
                ORIGINATE_ERROR("Out of memory");

            const float exposureCompensation =
                ((float)requestIndex / (float)(m_exposureSteps - 1)) *
                (m_exposureRange.get().max - m_exposureRange.get().min) + m_exposureRange.get().min;

            PROPAGATE_ERROR(expLevel->initialize(exposureCompensation));

            m_expLevels.push_back(expLevel.release());

            if (requestIndex != 0)
                message << ", ";
            message << exposureCompensation << " ev";
        }

        message << ". " << std::endl;
        Dispatcher::getInstance().message(message.str().c_str());
    }

    // activate the streams and populate the burst request array
    std::vector<const Argus::Request*> requests;
    Renderer &renderer = Renderer::getInstance();
    for (std::list<ExpLevel*>::iterator it = m_expLevels.begin(); it != m_expLevels.end(); ++it)
    {
        ExpLevel *expLevel = *it;
        PROPAGATE_ERROR(renderer.setStreamActive(
            Argus::interface_cast<Argus::IStream>(expLevel->m_outputStream)->getEGLStream(), true));
        requests.push_back(expLevel->m_request.get());
    }

    // start the repeating burst request for the preview
    PROPAGATE_ERROR(Dispatcher::getInstance().startRepeatBurst(requests));

    m_running = true;

    return true;
}

bool TaskMultiExposure::stop()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        return true;

    // stop the repeating burst request
    Dispatcher &dispatcher = Dispatcher::getInstance();
    PROPAGATE_ERROR(dispatcher.stopRepeat());

    // de-activate the streams
    Renderer &renderer = Renderer::getInstance();
    for (std::list<ExpLevel*>::iterator it = m_expLevels.begin(); it != m_expLevels.end(); ++it)
    {
        ExpLevel *expLevel = *it;
        PROPAGATE_ERROR(renderer.setStreamActive(
            Argus::interface_cast<Argus::IStream>(expLevel->m_outputStream)->getEGLStream(), false));
    }

    PROPAGATE_ERROR(dispatcher.waitForIdle());

    PROPAGATE_ERROR(shutdownExpLevels());

    m_running = false;

    return true;
}

}; // namespace ArgusSamples
