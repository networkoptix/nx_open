/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the
above copyright notice, this list of conditions
and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
nor the names of its contributors may be used to
endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
Yunhong Gu, last updated 01/01/2011
*****************************************************************************/

#include "epoll.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/epoll.h>
#elif __APPLE__
#include <unistd.h>
#include <sys/event.h>
#endif
#include <algorithm>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <iterator>

#include "common.h"
#include "epoll/epoll_impl.h"
#include "udt.h"

using namespace std;

CEPoll::CEPoll():
    m_iIDSeed(0)
{
}

CEPoll::~CEPoll()
{
}

Result<int> CEPoll::create()
{
    auto desc = std::make_unique<EpollImpl>();
    if (auto result = desc->initialize(); !result.ok())
        return result.error();

    std::lock_guard<std::mutex> pg(m_EPollLock);

    if (++m_iIDSeed >= 0x7FFFFFFF)
        m_iIDSeed = 0;

    m_mPolls[m_iIDSeed] = std::move(desc);

    return success(m_iIDSeed);
}

Result<> CEPoll::add_usock(const int eid, const UDTSOCKET& u, const int* events)
{
    std::lock_guard<std::mutex> pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        return Error(OsErrorCode::badDescriptor);

    p->second->addUdtSocket(u, events);
    return success();
}

Result<> CEPoll::add_ssock(const int eid, const SYSSOCKET& s, const int* events)
{
    std::lock_guard<std::mutex> pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        return Error(OsErrorCode::badDescriptor);

    p->second->add(s, events);

    return success();
}

Result<> CEPoll::remove_usock(const int eid, const UDTSOCKET& u)
{
    std::lock_guard<std::mutex> pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        return Error(OsErrorCode::badDescriptor);

    p->second->removeUdtSocket(u);
    return success();
}

Result<> CEPoll::remove_ssock(const int eid, const SYSSOCKET& s)
{
    std::lock_guard<std::mutex> pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        return Error(OsErrorCode::badDescriptor);

    p->second->remove(s);

    return success();
}

Result<int> CEPoll::wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds,
    int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    // if all fields is NULL and waiting time is infinite, then this would be a deadlock
    if (!readfds && !writefds && !lrfds && lwfds && (msTimeOut < 0))
        return Error(OsErrorCode::invalidData);

    //NOTE calls with same eid MUST be synchronized by caller!
    //That is, while we are in this method no calls with same eid are possible
    auto epollContext = getEpollById(eid);
    if (!epollContext.ok())
        return epollContext.error();

    return epollContext.get()->wait(readfds, writefds, msTimeOut, lrfds, lwfds);
}

Result<> CEPoll::interruptWait(const int eid)
{
    auto epollContext = getEpollById(eid);
    if (!epollContext.ok())
        return epollContext.error();

    epollContext.get()->interruptWait();
    return success();
}

Result<> CEPoll::release(const int eid)
{
    std::lock_guard<std::mutex> pg(m_EPollLock);

    CEPollDescMap::iterator i = m_mPolls.find(eid);
    if (i == m_mPolls.end())
        return Error(OsErrorCode::badDescriptor);

    m_mPolls.erase(i);
    return success();
}

int CEPoll::update_events(
    const UDTSOCKET& socketId,
    const std::set<int>& epollToTriggerIDs,
    int events,
    bool enable)
{
    std::lock_guard<std::mutex> lk(m_EPollLock);

    for (auto i = epollToTriggerIDs.begin(); i != epollToTriggerIDs.end(); ++i)
    {
        auto epollIter = m_mPolls.find(*i);
        if (epollIter == m_mPolls.end())
            continue;

        epollIter->second->updateEpollSets(events, socketId, enable);
    }

    return 0;
}

void CEPoll::RemoveEPollEvent(UDTSOCKET socket)
{
    std::lock_guard<std::mutex> pg(m_EPollLock);
    for (CEPollDescMap::iterator p = m_mPolls.begin(); p != m_mPolls.end(); ++p)
        p->second->removeUdtSocketEvents(socket);
}

Result<EpollImpl*> CEPoll::getEpollById(int eid) const
{
    std::lock_guard<std::mutex> lk(m_EPollLock);

    auto it = m_mPolls.find(eid);
    if (it == m_mPolls.end())
        return Error(OsErrorCode::badDescriptor);

    return success(it->second.get());
}
