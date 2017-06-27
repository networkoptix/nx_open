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

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "PerfTracker.h"
#include "Dispatcher.h"

namespace ArgusSamples
{

int PerfTracker::s_count = 0;
TimeValue PerfTracker::ms_appStartTime;
TimeValue PerfTracker::ms_appInitializedTime;

PerfTracker::PerfTracker()
    : m_id(s_count)
    , m_numberframesReceived(0)
    , m_lastFrameCount(0)
    , m_totalFrameDrop(0)
{
    s_count++;
}

void PerfTracker::onAppEvent(PerfEventType type, uint64_t value)
{
    switch(type)
    {
        case APP_START:
            ms_appStartTime = getCurrentTime();
            break;
        case APP_INITIALIZED:
            ms_appInitializedTime = getCurrentTime();
            break;
        default:
            printf("unexpected type in perfTracker\n");
            break;
    }
    return;
}

void PerfTracker::onEvent(PerfEventType type, uint64_t value)
{
    if (!Dispatcher::getInstance().m_kpi)
        return;

    switch(type)
    {
        case TASK_START:
            m_taskStartTime = getCurrentTime();
            printf("PerfTracker: app initial %" PRIu64 " ms\n",
                (ms_appInitializedTime - ms_appStartTime).toMSec());
            printf("PerfTracker %d: app intialized to task start %" PRIu64 " ms\n", m_id,
                (m_taskStartTime - ms_appInitializedTime).toMSec());
            break;
        case ISSUE_CAPTURE:
            m_issueCaptureTime = getCurrentTime();
            printf("PerfTracker %d: task start to issue capture %" PRIu64 " ms\n", m_id,
                (m_issueCaptureTime - m_taskStartTime).toMSec());
            break;
        case REQUEST_RECEIVED:
            m_requestReceivedTime = getCurrentTime();
            if (m_numberframesReceived == 0)
            {
                m_firstRequestReceivedTime = m_requestReceivedTime;
                printf("PerfTracker %d: first request %" PRIu64 " ms\n", m_id,
                    (m_firstRequestReceivedTime - m_issueCaptureTime).toMSec());
                printf("PerfTracker %d: total launch time %" PRIu64 " ms\n", m_id,
                    (m_firstRequestReceivedTime - ms_appStartTime).toMSec());
            }

            m_numberframesReceived++;
            if ((m_numberframesReceived % 30) == 2)
            {
                float frameRate =
                    static_cast<float>(m_numberframesReceived - 1) *
                    (m_requestReceivedTime - m_firstRequestReceivedTime).toCyclesPerSec();
                printf("PerfTracker %d: frameRate %.2f frames per second\n", m_id, frameRate);
            }
            break;
        case REQUEST_LATENCY:
            // can do some stats
            printf("PerfTracker %d: latency %" PRIu64 " ms\n", m_id, value);
            break;
        case FRAME_COUNT:
            {
                uint64_t currentFrameCount = 0;
                uint64_t currentFrameDrop = 0;
                currentFrameCount = value;
                // start frame drop count from 2nd frame
                if (m_lastFrameCount > 0)
                {
                    currentFrameDrop = currentFrameCount - m_lastFrameCount - 1;
                    m_totalFrameDrop += currentFrameDrop;
                }
                printf("perfTracker %d: framedrop current request %" PRIu64 ", total %" PRIu64 "\n",
                          m_id, currentFrameDrop, m_totalFrameDrop);
                m_lastFrameCount = currentFrameCount;
                break;
            }
        case CLOSE_REQUESTED:
            m_closeRequestedTime = getCurrentTime();
            break;
        case FLUSH_DONE:
            m_flushDoneTime = getCurrentTime();
            printf("PerfTracker %d: flush takes %" PRIu64 " ms\n", m_id,
                (m_flushDoneTime - m_closeRequestedTime).toMSec());
            break;
        case CLOSE_DONE:
            m_closeDoneTime = getCurrentTime();
            printf("PerfTracker %d: device close takes %" PRIu64 " ms\n", m_id,
                (m_closeDoneTime - m_flushDoneTime).toMSec());
            printf("PerfTracker %d: total close takes %" PRIu64 " ms\n", m_id,
                (m_closeDoneTime - m_closeRequestedTime).toMSec());
            break;

        default:
            printf("unexpected type in perfTracker\n");
            break;
    }
    return;
}

}; // namespace ArgusSamples
