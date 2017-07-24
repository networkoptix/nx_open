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

#include "EventThread.h"
#include "Dispatcher.h"
#include "Util.h"
#include <Argus/Ext/DebugCaptureMetadata.h>

namespace ArgusSamples {

EventThread::EventThread(Argus::CaptureSession *session,
                         Argus::EventQueue *eventQueue,
                         PerfTracker *perftracker)
    : m_session(session)
    , m_eventQueue(eventQueue)
    , m_perfTracker(perftracker)
{
}

EventThread::~EventThread()
{
}

bool EventThread::threadInitialize()
{
    return true;
}

bool EventThread::threadExecute()
{
    Dispatcher &dispatcher = Dispatcher::getInstance();

    // wait for events (use a time out to allow the thread to be shutdown even if there are no
    // new events)
    PROPAGATE_ERROR(dispatcher.waitForEvents(m_eventQueue.get(), TimeValue::fromMSec(100),
        m_session));

    Argus::IEventQueue *iEventQueue =
        Argus::interface_cast<Argus::IEventQueue>(m_eventQueue.get());
    if (!iEventQueue)
        ORIGINATE_ERROR("Failed to get iEventQueue");

    for (uint32_t i = 0; i < iEventQueue->getSize(); i++)
    {
        const Argus::Event *event = iEventQueue->getEvent(i);
        const Argus::IEvent *ievent = Argus::interface_cast<const Argus::IEvent>(event);
        if (!ievent)
            ORIGINATE_ERROR("Failed to get ievent interface");

        if (ievent->getEventType() == Argus::EVENT_TYPE_CAPTURE_COMPLETE)
        {
            m_perfTracker->onEvent(REQUEST_RECEIVED);
            uint64_t latencyMs = 0;

            const Argus::IEventCaptureComplete *ieventCaptureComplete
                 = Argus::interface_cast<const Argus::IEventCaptureComplete>(event);
            const Argus::CaptureMetadata *metaData = ieventCaptureComplete->getMetadata();
            if (metaData)
            {
                const Argus::ICaptureMetadata *iCaptureMeta =
                          Argus::interface_cast<const Argus::ICaptureMetadata>(metaData);
                if (iCaptureMeta)
                {
                    latencyMs = ievent->getTime()/1000 - iCaptureMeta->getSensorTimestamp()/1000000;
                    m_perfTracker->onEvent(REQUEST_LATENCY, latencyMs);
                }

                const Argus::Ext::IDebugCaptureMetadata *iDebugCaptureMeta =
                    Argus::interface_cast<const Argus::Ext::IDebugCaptureMetadata>(metaData);
                if (iDebugCaptureMeta)
                {
                    uint64_t currentFrameCount = iDebugCaptureMeta->getHwFrameCount();
                    m_perfTracker->onEvent(FRAME_COUNT, currentFrameCount);
                }

            }
        }
    }

    return true;
}

bool EventThread::threadShutdown()
{
    return true;
}

}; // namespace ArgusSamples
