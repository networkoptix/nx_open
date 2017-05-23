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

#ifndef PERFTRACKER_H
#define PERFTRACKER_H

#include "Util.h" // for TimeValue

namespace ArgusSamples
{

typedef enum
{
    APP_START,
    APP_INITIALIZED,
    TASK_START,
    ISSUE_CAPTURE,
    REQUEST_RECEIVED,
    REQUEST_LATENCY,
    FRAME_COUNT,
    CLOSE_REQUESTED,
    FLUSH_DONE,
    CLOSE_DONE
} PerfEventType;

/**
 * PerfTracker note down the perf for one session
 */

class PerfTracker
{
public:
    PerfTracker();
    void onEvent(PerfEventType type, uint64_t value = 0);
    static void onAppEvent(PerfEventType type, uint64_t value = 0);

private:
    static int s_count; // used for multi-session
    int m_id;
    static TimeValue ms_appStartTime;
    static TimeValue ms_appInitializedTime;

    TimeValue m_taskStartTime;
    TimeValue m_issueCaptureTime;
    TimeValue m_requestReceivedTime;

    TimeValue m_firstRequestReceivedTime;
    uint64_t m_numberframesReceived;

    TimeValue m_closeRequestedTime;
    TimeValue m_flushDoneTime;
    TimeValue m_closeDoneTime;

    uint64_t m_lastFrameCount;
    uint64_t m_totalFrameDrop;
};

}; // namespace ArgusSamples

#endif //PERFTRACKER_H

