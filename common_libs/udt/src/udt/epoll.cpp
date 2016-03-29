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

#ifdef __linux__
#include <sys/epoll.h>
#include <unistd.h>
#elif __APPLE__
#include <sys/event.h>
#endif
#include <algorithm>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <iterator>

#include "common.h"
#include "epoll.h"
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

    int localid = 0;

#if __linux__
    localid = epoll_create(1024);    //Since Linux 2.6.8, the size argument is ignored, but must be greater than zero
    if (localid < 0)
        throw CUDTException(-1, 0, errno);
#elif __APPLE__
    localid = kqueue();
    if (localid < 0)
        throw CUDTException(-1, 0, errno);
#elif _WIN32
    // on BSD, use kqueue
    // on Solaris, use /dev/poll
    // on Windows, select
#endif

    if (++m_iIDSeed >= 0x7FFFFFFF)
        m_iIDSeed = 0;

    CEPollDesc desc;
    desc.m_iID = m_iIDSeed;
    desc.m_iLocalID = localid;
    m_mPolls[desc.m_iID] = desc;

    return desc.m_iID;
}

int CEPoll::add_usock(const int eid, const UDTSOCKET& u, const int* events)
{
    CGuard pg(m_EPollLock);

    map<int, CEPollDesc>::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    if (!events || (*events & UDT_EPOLL_IN))
        p->second.m_sUDTSocksIn.insert(u);
    if (!events || (*events & UDT_EPOLL_OUT))
        p->second.m_sUDTSocksOut.insert(u);

    return 0;
}

int CEPoll::add_ssock(const int eid, const SYSSOCKET& s, const int* events)
{
    CGuard pg(m_EPollLock);

    map<int, CEPollDesc>::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

#if __linux__
    epoll_event ev;
    memset(&ev, 0, sizeof(epoll_event));

    if (NULL == events)
        ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
    else
    {
        ev.events = 0;
        if (*events & UDT_EPOLL_IN)
            ev.events |= EPOLLIN;
        if (*events & UDT_EPOLL_OUT)
            ev.events |= EPOLLOUT;
        if (*events & UDT_EPOLL_ERR)
            ev.events |= EPOLLERR;
    }

    ev.data.fd = s;
    if (::epoll_ctl(p->second.m_iLocalID, EPOLL_CTL_ADD, s, &ev) < 0)
        throw CUDTException();
#elif __APPLE__
    struct kevent ev[2];
    size_t evCount = 0;
    if (NULL == events)
    {
        EV_SET(&(ev[evCount++]), s, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        EV_SET(&(ev[evCount++]), s, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    else
    {
        if (*events & UDT_EPOLL_IN)
            EV_SET(&(ev[evCount++]), s, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        if (*events & UDT_EPOLL_OUT)
            EV_SET(&(ev[evCount++]), s, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }

    //adding new fd to set
    if (kevent(p->second.m_iLocalID, ev, evCount, NULL, 0, NULL) != 0)
        throw CUDTException();
#elif _WIN32
#endif

    int& eventMask = p->second.m_sLocals[s];
    eventMask |= *events;

    return 0;
}

int CEPoll::remove_usock(const int eid, const UDTSOCKET& u)
{
    CGuard pg(m_EPollLock);

    map<int, CEPollDesc>::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    p->second.m_sUDTSocksIn.erase(u);
    p->second.m_sUDTSocksOut.erase(u);
    p->second.m_sUDTSocksEx.erase(u);

    return 0;
}

int CEPoll::remove_ssock(const int eid, const SYSSOCKET& s)
{
    CGuard pg(m_EPollLock);

    map<int, CEPollDesc>::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

#if __linux__
    epoll_event ev;  // ev is ignored, for compatibility with old Linux kernel only.
    if (::epoll_ctl(p->second.m_iLocalID, EPOLL_CTL_DEL, s, &ev) < 0)
        throw CUDTException();
#elif __APPLE__
    struct kevent ev[2];
    EV_SET(&(ev[0]), s, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&(ev[1]), s, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(p->second.m_iLocalID, ev, 2, NULL, 0, NULL);  //ignoring return code, since event is removed in any case
#elif _WIN32
#endif

    p->second.m_sLocals.erase(s);

    return 0;
}

int CEPoll::wait(const int eid, set<UDTSOCKET>* readfds, set<UDTSOCKET>* writefds, int64_t msTimeOut, set<SYSSOCKET>* lrfds, set<SYSSOCKET>* lwfds)
{
    CEPollDesc* epollContext = nullptr;
    {
        //NOTE calls with same eid MUST be synchronized by caller!
        //That is, while we are in this method no calls with same eid are possible
        CGuard lk(m_EPollLock);

        map<int, CEPollDesc>::iterator it = m_mPolls.find(eid);
        if (it == m_mPolls.end())
            throw CUDTException(5, 13);
        epollContext = &it->second;
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

    int64_t entertime = CTimer::getTime();
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
                *readfds = epollContext->m_sUDTReads;
                for (set<UDTSOCKET>::const_iterator i = epollContext->m_sUDTExcepts.begin(); i != epollContext->m_sUDTExcepts.end(); ++i)
                    readfds->insert(*i);
                total += epollContext->m_sUDTReads.size() + epollContext->m_sUDTExcepts.size();
            }
            if ((NULL != writefds) && (!epollContext->m_sUDTWrites.empty() || !epollContext->m_sUDTExcepts.empty()))
            {
                *writefds = epollContext->m_sUDTWrites;
                for (set<UDTSOCKET>::const_iterator i = epollContext->m_sUDTExcepts.begin(); i != epollContext->m_sUDTExcepts.end(); ++i)
                    writefds->insert(*i);
                total += epollContext->m_sUDTWrites.size() + epollContext->m_sUDTExcepts.size();
            }
        }

        if (lrfds || lwfds)
        {
#if __linux__ 
            const int max_events = 1024;
            epoll_event ev[max_events];
            int nfds = ::epoll_wait(epollContext->m_iLocalID, ev, max_events, 0);

            for (int i = 0; i < nfds; ++i)
            {
                if ((NULL != lrfds) && (ev[i].events & EPOLLIN))
                {
                    lrfds->insert(ev[i].data.fd);
                    ++total;
                }
                if ((NULL != lwfds) && (ev[i].events & EPOLLOUT))
                {
                    lwfds->insert(ev[i].data.fd);
                    ++total;
                }
            }
#elif __APPLE__
            static const size_t MAX_EVENTS_TO_READ = 128;
            struct kevent ev[MAX_EVENTS_TO_READ];

            memset(ev, 0, sizeof(ev));
            struct timespec timeout;
            if (msTimeOut >= 0)
            {
                memset(&timeout, 0, sizeof(timeout));
                timeout.tv_sec = msTimeOut / MILLIS_IN_SEC;
                timeout.tv_nsec = (msTimeOut % MILLIS_IN_SEC) * NSEC_IN_MS;
            }
            int nfds = kevent(
                epollContext->m_iLocalID,
                NULL,
                0,
                ev,
                MAX_EVENTS_TO_READ,
                msTimeOut >= 0 ? &timeout : NULL);

            for (int i = 0; i < nfds; ++i)
            {
                if ((NULL != lrfds) && (ev[i].filter == EVFILT_READ))
                {
                    lrfds->insert(ev[i].ident);
                    ++total;
                }
                if ((NULL != lwfds) && (ev[i].filter == EVFILT_WRITE))
                {
                    lwfds->insert(ev[i].ident);
                    ++total;
                }
            }
#elif _WIN32
            //currently "select" is used for all non-Linux platforms.
            //faster approaches can be applied for specific systems in the future.

            //"select" has a limitation on the number of sockets

            //TODO #ak remove limitation on select set size
            fd_set readfds;
            fd_set writefds;
            fd_set exceptfds;
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);

            for (map<SYSSOCKET, int>::const_iterator i = epollContext->m_sLocals.begin(); i != epollContext->m_sLocals.end(); ++i)
            {
                if (lrfds && (i->second & UDT_EPOLL_IN) > 0)
                    FD_SET(i->first, &readfds);
                if (lwfds && (i->second & UDT_EPOLL_OUT) > 0)
                    FD_SET(i->first, &writefds);
                //TODO #ak handle exceptfds
                //FD_SET(i->first, &exceptfds);
            }

            timeval tv;
            memset(&tv, 0, sizeof(tv));

            const int eventCount = ::select(
                0,
                readfds.fd_count > 0 ? &readfds : NULL,
                writefds.fd_count > 0 ? &writefds : NULL,
                exceptfds.fd_count > 0 ? &exceptfds : NULL,
                &tv);
            if (eventCount < 0)
                return -1;
            if (eventCount > 0)
            {
                //TODO #ak use win32-specific select features to get O(1) here
                for (map<SYSSOCKET, int>::const_iterator i = epollContext->m_sLocals.begin(); i != epollContext->m_sLocals.end(); ++i)
                {
                    if (lrfds && FD_ISSET(i->first, &readfds))
                    {
                        lrfds->insert(i->first);
                        ++total;
                    }
                    if (lwfds && FD_ISSET(i->first, &writefds))
                    {
                        lwfds->insert(i->first);
                        ++total;
                    }
                }
            }
#endif
        }

        if (total > 0)
            return total;

        if ((msTimeOut >= 0) && (int64_t(CTimer::getTime() - entertime) >= msTimeOut * 1000LL))
            //throw CUDTException(6, 3, 0);
            return 0; //on timeout epoll_wait MUST return 0!

        CTimer::waitForEvent();
    }

    return 0;
}

int CEPoll::release(const int eid)
{
    CGuard pg(m_EPollLock);

    map<int, CEPollDesc>::iterator i = m_mPolls.find(eid);
    if (i == m_mPolls.end())
        throw CUDTException(5, 13);

#if __linux__ || __APPLE__ 
    // release local/system epoll descriptor
    ::close(i->second.m_iLocalID);
#elif _WIN32
#endif

    m_mPolls.erase(i);

    return 0;
}

namespace
{

void update_epoll_sets(const UDTSOCKET& uid, const set<UDTSOCKET>& watch, set<UDTSOCKET>& result, bool enable)
{
    if (enable && (watch.find(uid) != watch.end()))
    {
        result.insert(uid);
    }
    else if (!enable)
    {
        result.erase(uid);
    }
}

}  // namespace

int CEPoll::update_events(const UDTSOCKET& uid, const std::set<int>& eids, int events, bool enable)
{
    CGuard lk(m_EPollLock);

    for (set<int>::iterator i = eids.begin(); i != eids.end(); ++i)
    {
        auto p = m_mPolls.find(*i);
        if (p == m_mPolls.end())
        {
            continue;
        }
        else
        {
            if ((events & UDT_EPOLL_IN) != 0)
                update_epoll_sets(uid, p->second.m_sUDTSocksIn, p->second.m_sUDTReads, enable);
            if ((events & UDT_EPOLL_OUT) != 0)
                update_epoll_sets(uid, p->second.m_sUDTSocksOut, p->second.m_sUDTWrites, enable);
            if ((events & UDT_EPOLL_ERR) != 0)
                update_epoll_sets(uid, p->second.m_sUDTSocksEx, p->second.m_sUDTExcepts, enable);
        }
    }
    lk.unlock();

    CTimer::triggerEvent();

    return 0;
}

void CEPoll::RemoveEPollEvent(UDTSOCKET socket)
{
    CGuard pg(m_EPollLock);
    map<int, CEPollDesc>::iterator p;
    for (p = m_mPolls.begin(); p != m_mPolls.end(); ++p)
    {
        p->second.m_sUDTReads.erase(socket);
        p->second.m_sUDTWrites.erase(socket);
        p->second.m_sUDTExcepts.erase(socket);
    }
}
