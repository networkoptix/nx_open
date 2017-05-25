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

#ifndef TASK_MULTI_SESSION_H
#define TASK_MULTI_SESSION_H

#include <list>

#include <Argus/Argus.h>

#include "ITask.h"
#include "Util.h"
#include "PerfTracker.h"
#include "UniquePointer.h"

namespace ArgusSamples
{
class EventThread;

/**
 * This task creates one session for each available sensor
 */
class TaskMultiSession : public ITask
{
public:
    TaskMultiSession();
    virtual ~TaskMultiSession();

    /** @name ITask methods */
    /**@{*/
    virtual bool initialize();
    virtual bool shutdown();
    virtual bool start();
    virtual bool stop();
    /**@}*/

private:
    bool m_initialized;                 ///< set if initialized
    bool m_running;                     ///< set if preview is running

    /**
     * For each device there is one session with a request. Each request outputs to a stream which
     * is rendered.
     */
    class Session
    {
    public:
        Session();
        ~Session();

        bool shutdown();
        bool stop();
        bool initialize(uint32_t deviceIndex);

        Argus::UniqueObj<Argus::CaptureSession> m_session;      ///< Argus session
        Argus::UniqueObj<Argus::Request> m_request;             ///< Argus request
        Argus::UniqueObj<Argus::OutputStream> m_outputStream;   ///< Argus output stream

        UniquePointer<EventThread> m_eventThread;
        PerfTracker m_perfTracker;
    };

    std::list<Session*> m_sessions;

    bool shutdownSessions();
};

}; // namespace ArgusSamples

#endif // TASK_MULTI_SESSION_H
