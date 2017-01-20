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
#include "epoll/epoll_impl_factory.h"
#include "udt.h"

using namespace std;


static const int MILLIS_IN_SEC = 1000;
static const int NSEC_IN_MS = 1000;

CEPoll::CEPoll():
    m_iIDSeed(0)
{
    CGuard::createMutex(m_EPollLock);
}

CEPoll::~CEPoll()
{
    CGuard::releaseMutex(m_EPollLock);
}

int CEPoll::create()
{
    CGuard pg(m_EPollLock);

    auto desc = EpollImplFactory::instance()->create();

    if (++m_iIDSeed >= 0x7FFFFFFF)
        m_iIDSeed = 0;

    m_mPolls[m_iIDSeed] = std::move(desc);

    return m_iIDSeed;
}

int CEPoll::add_usock(const int eid, const UDTSOCKET& u, const int* events)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    return p->second->add_usock(u, events);
}

int CEPoll::add_ssock(const int eid, const SYSSOCKET& s, const int* events)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    p->second->add(s, events);

    return 0;
}

int CEPoll::remove_usock(const int eid, const UDTSOCKET& u)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    return p->second->remove_usock(u);
}

int CEPoll::remove_ssock(const int eid, const SYSSOCKET& s)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    p->second->remove(s);

    return 0;
}

int CEPoll::wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    EpollImpl* epollContext = nullptr;
    {
        //NOTE calls with same eid MUST be synchronized by caller!
        //That is, while we are in this method no calls with same eid are possible
        CGuard lk(m_EPollLock);

        CEPollDescMap::iterator it = m_mPolls.find(eid);
        if (it == m_mPolls.end())
            throw CUDTException(5, 13);
        epollContext = it->second.get();
    }


    // if all fields is NULL and waiting time is infinite, then this would be a deadlock
    if (!readfds && !writefds && !lrfds && lwfds && (msTimeOut < 0))
        throw CUDTException(5, 3, 0);

    // Clear these sets in case the app forget to do it.
    if (readfds) readfds->clear();
    if (writefds) writefds->clear();
    if (lrfds) lrfds->clear();
    if (lwfds) lwfds->clear();

    int total = 0;

    uint64_t entertime = CTimer::getTime();
    while (true)
    {
        {
            CGuard lk(m_EPollLock);
            if (epollContext->m_sUDTSocksIn.empty() && epollContext->m_sUDTSocksOut.empty() && epollContext->m_sLocals.empty() && (msTimeOut < 0))
            {
                // no socket is being monitored, this may be a deadlock
                throw CUDTException(5, 3);
            }

            // Sockets with exceptions are returned to both read and write sets.
            if ((NULL != readfds) && (!epollContext->m_sUDTReads.empty() || !epollContext->m_sUDTExcepts.empty()))
            {
                readfds->clear();
                for (const auto& handle: epollContext->m_sUDTReads)
                    readfds->emplace(handle, UDT_EPOLL_IN);
                for (set<UDTSOCKET>::const_iterator i = epollContext->m_sUDTExcepts.begin(); i != epollContext->m_sUDTExcepts.end(); ++i)
                    (*readfds)[*i] |= UDT_EPOLL_ERR;
                total += epollContext->m_sUDTReads.size() + epollContext->m_sUDTExcepts.size();
            }
            if ((NULL != writefds) && (!epollContext->m_sUDTWrites.empty() || !epollContext->m_sUDTExcepts.empty()))
            {
                for (const auto& handle : epollContext->m_sUDTWrites)
                    writefds->emplace(handle, UDT_EPOLL_OUT);
                for (set<UDTSOCKET>::const_iterator i = epollContext->m_sUDTExcepts.begin(); i != epollContext->m_sUDTExcepts.end(); ++i)
                    (*writefds)[*i] |= UDT_EPOLL_ERR;
                total += epollContext->m_sUDTWrites.size() + epollContext->m_sUDTExcepts.size();
            }

            lk.unlock();

            for (auto& socketHandleAndEventMask: *writefds)
            {
                if ((socketHandleAndEventMask.second & UDT_EPOLL_ERR) > 0)
                    continue;

                //somehow, connection failure event can be not reported
                int socketEventMask = 0;
                int socketEventMaskSize = sizeof(socketEventMask);
                const int result = UDT::getsockopt(
                    socketHandleAndEventMask.first,
                    0,
                    UDT_EVENT,
                    &socketEventMask,
                    &socketEventMaskSize);
                if (result == 0)
                {
                    if ((socketEventMask & UDT_EPOLL_ERR) > 0)
                        socketHandleAndEventMask.second |= UDT_EPOLL_ERR;
                }
            }
        }

        if (lrfds || lwfds)
        {
            const int eventCount = epollContext->doSystemPoll(lrfds, lwfds);
            if (eventCount < 0)
                return -1;
            total += eventCount;
        }

        if (total > 0)
            return total;

        auto now = CTimer::getTime();
        if (msTimeOut >= 0 && now - entertime >= (uint64_t) msTimeOut * 1000)
            return 0;

        CTimer::waitForEvent();
    }

    return 0;
}

int CEPoll::release(const int eid)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator i = m_mPolls.find(eid);
    if (i == m_mPolls.end())
        throw CUDTException(5, 13);

    m_mPolls.erase(i);

    return 0;
}

int CEPoll::update_events(
    const UDTSOCKET& socketId,
    const std::set<int>& epollToTriggerIDs,
    int events,
    bool enable)
{
    CGuard lk(m_EPollLock);

    for (set<int>::iterator i = epollToTriggerIDs.begin(); i != epollToTriggerIDs.end(); ++i)
    {
        auto epollIter = m_mPolls.find(*i);
        if (epollIter == m_mPolls.end())
            continue;

        epollIter->second->updateEpollSets(events, socketId, enable);
    }
    lk.unlock();

    return 0;
}

void CEPoll::RemoveEPollEvent(UDTSOCKET socket)
{
    CGuard pg(m_EPollLock);
    for (CEPollDescMap::iterator p = m_mPolls.begin(); p != m_mPolls.end(); ++p)
        p->second->removeSocketEvents(socket);
}
