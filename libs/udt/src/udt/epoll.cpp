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
    CGuard::createMutex(m_EPollLock);
}

CEPoll::~CEPoll()
{
    CGuard::releaseMutex(m_EPollLock);
}

int CEPoll::create()
{
    CGuard pg(m_EPollLock);

    std::unique_ptr<EpollImpl> desc(new EpollImpl());

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

    return p->second->addUdtSocket(u, events);
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

    return p->second->removeUdtSocket(u);
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
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds,
    int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    // if all fields is NULL and waiting time is infinite, then this would be a deadlock
    if (!readfds && !writefds && !lrfds && lwfds && (msTimeOut < 0))
        throw CUDTException(5, 3, 0);

    //NOTE calls with same eid MUST be synchronized by caller!
    //That is, while we are in this method no calls with same eid are possible
    EpollImpl* epollContext = getEpollById(eid);
    return epollContext->wait(readfds, writefds, msTimeOut, lrfds, lwfds);
}

int CEPoll::interruptWait(const int eid)
{
    return getEpollById(eid)->interruptWait();
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
        p->second->removeUdtSocketEvents(socket);
}

EpollImpl* CEPoll::getEpollById(int eid) const
{
    CGuard lk(m_EPollLock);

    auto it = m_mPolls.find(eid);
    if (it == m_mPolls.end())
        throw CUDTException(5, 13);
    return it->second.get();
}
