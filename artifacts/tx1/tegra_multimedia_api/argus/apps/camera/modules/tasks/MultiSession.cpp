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

#include "MultiSession.h"
#include "Renderer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "UniquePointer.h"
#include "EventThread.h"

namespace ArgusSamples
{

TaskMultiSession::TaskMultiSession()
    : m_initialized(false)
    , m_running(false)
{
}

TaskMultiSession::~TaskMultiSession()
{
    shutdown();
}

TaskMultiSession::Session::Session()
    : m_eventThread(NULL)
{
}

TaskMultiSession::Session::~Session()
{
    shutdown();
}

bool TaskMultiSession::Session::initialize(uint32_t deviceIndex)
{
    m_perfTracker.onEvent(TASK_START);
    Renderer &renderer = Renderer::getInstance();

    Dispatcher &dispatcher = Dispatcher::getInstance();

    // create the session using the current device index
    PROPAGATE_ERROR(dispatcher.createSession(m_session, deviceIndex));

    // create the request
    PROPAGATE_ERROR(dispatcher.createRequest(m_request, Argus::CAPTURE_INTENT_STILL_CAPTURE,
        m_session.get()));

    // Create the preview stream
    PROPAGATE_ERROR(dispatcher.createOutputStream(m_request.get(), m_outputStream,
        m_session.get()));

    // bind the preview stream to the renderer
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

    std::vector<Argus::EventType> eventTypes;
    eventTypes.push_back(Argus::EVENT_TYPE_CAPTURE_COMPLETE);

    Argus::UniqueObj<Argus::EventQueue> eventQueue;
    PROPAGATE_ERROR(dispatcher.createEventQueue(eventTypes, eventQueue, m_session.get()));

    // pass ownership of eventQueue to EventThread
    m_eventThread.reset(new EventThread(m_session.get(), eventQueue.release(), &m_perfTracker));
    if (!m_eventThread)
    {
        ORIGINATE_ERROR("Failed to allocate EventThread");
    }

    PROPAGATE_ERROR(m_eventThread->initialize());
    PROPAGATE_ERROR(m_eventThread->waitRunning());

    return true;
}

bool TaskMultiSession::Session::stop()
{
    m_perfTracker.onEvent(CLOSE_REQUESTED);

    // stop eventThread first to avoid the thread keep waiting for long time to stop
    PROPAGATE_ERROR(m_eventThread->shutdown());
    m_eventThread.reset();

    Dispatcher &dispatcher = Dispatcher::getInstance();
    Renderer &renderer = Renderer::getInstance();

    PROPAGATE_ERROR(dispatcher.stopRepeat(m_session.get()));
    PROPAGATE_ERROR(renderer.setStreamActive(
        Argus::interface_cast<Argus::IStream>(m_outputStream)->getEGLStream(), false));
    PROPAGATE_ERROR(dispatcher.waitForIdle(m_session.get()));
    m_perfTracker.onEvent(FLUSH_DONE);

    return true;
}

bool TaskMultiSession::Session::shutdown()
{
    if (m_request)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        if (m_outputStream)
        {
            // destroy the producer
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

    m_perfTracker.onEvent(CLOSE_DONE);
    // Destroy the session
    m_session.reset();

    return true;
}

bool TaskMultiSession::initialize()
{
    if (m_initialized)
        return true;

    m_initialized = true;

    return true;
}

bool TaskMultiSession::shutdown()
{
    if (!m_initialized)
        return true;

    // stop the preview
    PROPAGATE_ERROR(stop());

    m_initialized = false;

    return true;
}

bool TaskMultiSession::shutdownSessions()
{
    if (!m_sessions.empty())
    {
        // shutdown the sessions
        for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
        {
            Session *session = *it;
            PROPAGATE_ERROR_CONTINUE(session->shutdown());
            delete session;
        }
        m_sessions.clear();
    }

    return true;
}

bool TaskMultiSession::start()
{
    if (m_running)
        return true;

    Dispatcher &dispatcher = Dispatcher::getInstance();

    if (m_sessions.empty())
    {
        const uint32_t deviceCount = dispatcher.getDeviceCount();

        if (deviceCount == 0)
            ORIGINATE_ERROR("No camera devices found");

        // create a request and streams for each session
        for (uint32_t deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex)
        {
            UniquePointer<Session> session(new Session);

            if (!session)
                ORIGINATE_ERROR("Out of memory");

            PROPAGATE_ERROR(session->initialize(deviceIndex));

            m_sessions.push_back(session.release());
        }
    }

    // activate the streams and populate the burst request array
    Renderer &renderer = Renderer::getInstance();
    for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
    {
        Session *session = *it;
        PROPAGATE_ERROR(renderer.setStreamActive(
            Argus::interface_cast<Argus::IStream>(session->m_outputStream)->getEGLStream(), true));

        PerfTracker &perf = session->m_perfTracker;
        perf.onEvent(ISSUE_CAPTURE);
        // start the repeating burst request for the preview
        PROPAGATE_ERROR(dispatcher.startRepeat(session->m_request.get(), session->m_session.get()));
    }

    m_running = true;

    return true;
}

bool TaskMultiSession::stop()
{
    if (!m_running)
        return true;

    for (std::list<Session*>::iterator it = m_sessions.begin(); it != m_sessions.end(); ++it)
    {
        Session *session = *it;
        PROPAGATE_ERROR(session->stop());
    }

    PROPAGATE_ERROR(shutdownSessions());

    m_running = false;

    return true;
}

}; // namespace ArgusSamples
