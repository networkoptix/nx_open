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
#include "epoll.h"
#include "udt.h"

using namespace std;


static const int MILLIS_IN_SEC = 1000;
static const int NSEC_IN_MS = 1000;

#ifdef _WIN32
namespace {

constexpr static const size_t kInitialFdSetSize = 1024;
constexpr static const size_t kFdSetIncreaseStep = 1024;

typedef struct CustomFdSet
{
    u_int fd_count;               /* how many are SET? */
                                  //!an array of SOCKETs. Actually, this array has size \a PollSetImpl::fdSetSize
    SOCKET fd_array[kInitialFdSetSize];
} CustomFdSet;

CustomFdSet* allocFDSet(size_t setSize)
{
    //NOTE: SOCKET is a pointer
    return (CustomFdSet*)malloc(sizeof(CustomFdSet) +
        sizeof(SOCKET)*(setSize - kInitialFdSetSize));
}

void freeFDSet(CustomFdSet* fdSet)
{
    ::free(fdSet);
}

/** Reallocs \a fdSet to be of size \a newSize while preseving existing data.
In case of allocation error \a *fdSet is freed and set to NULL
*/
void reallocFDSet(CustomFdSet** fdSet, size_t newSize)
{
    CustomFdSet* newSet = allocFDSet(newSize);

    //copying existing data
    if (*fdSet && newSet)
    {
        newSet->fd_count = (*fdSet)->fd_count;
        memcpy(newSet->fd_array, (*fdSet)->fd_array, (*fdSet)->fd_count*sizeof(*((*fdSet)->fd_array)));
    }

    freeFDSet(*fdSet);
    *fdSet = newSet;
}

void reallocFdSetIfNeeded(
    size_t desiredSize,
    size_t* currentCapacity,
    CustomFdSet** fdSet)
{
    if (*currentCapacity < desiredSize)
    {
        *currentCapacity = desiredSize + kFdSetIncreaseStep;
        reallocFDSet(fdSet, *currentCapacity);
    }
}

}
#endif  //_WIN32


class CEPollDesc
{
public:
    int m_iID;                                // epoll ID
    std::set<UDTSOCKET> m_sUDTSocksOut;       // set of UDT sockets waiting for write events
    std::set<UDTSOCKET> m_sUDTSocksIn;        // set of UDT sockets waiting for read events
    std::set<UDTSOCKET> m_sUDTSocksEx;        // set of UDT sockets waiting for exceptions

    int m_iLocalID;                           // local system epoll ID
    std::map<SYSSOCKET, int> m_sLocals;       // map<local (non-UDT) descriptor, event mask (UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR)>

    std::set<UDTSOCKET> m_sUDTWrites;         // UDT sockets ready for write
    std::set<UDTSOCKET> m_sUDTReads;          // UDT sockets ready for read
    std::set<UDTSOCKET> m_sUDTExcepts;        // UDT sockets with exceptions (connection broken, etc.)

    CEPollDesc()
    :
        m_iID(0),
        m_iLocalID(0)
    {
    }
};

#ifdef _WIN32
class CEPollDescWin32
:
    public CEPollDesc
{
public:
    CustomFdSet* readfds;
    size_t readfdsCapacity;
    CustomFdSet* writefds;
    size_t writefdsCapacity;
    CustomFdSet* exceptfds;
    size_t exceptfdsCapacity;

    CEPollDescWin32()
    :
        readfds(allocFDSet(kInitialFdSetSize)),
        readfdsCapacity(kInitialFdSetSize),
        writefds(allocFDSet(kInitialFdSetSize)),
        writefdsCapacity(kInitialFdSetSize),
        exceptfds(allocFDSet(kInitialFdSetSize)),
        exceptfdsCapacity(kInitialFdSetSize)
    {
    }

    ~CEPollDescWin32()
    {
        freeFDSet(readfds);
        readfds = NULL;
        freeFDSet(writefds);
        writefds = NULL;
        freeFDSet(exceptfds);
        exceptfds = NULL;
    }

private:
    CEPollDescWin32(const CEPollDescWin32&);
    CEPollDescWin32& operator=(const CEPollDescWin32&);
};
#endif


CEPoll::CEPoll()
:
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

#ifdef _WIN32
    std::unique_ptr<CEPollDescWin32> desc(new CEPollDescWin32());
#else
    std::unique_ptr<CEPollDesc> desc(new CEPollDesc());
#endif
    desc->m_iID = m_iIDSeed;
    desc->m_iLocalID = localid;
    m_mPolls[m_iIDSeed] = std::move(desc);

    return m_iIDSeed;
}

int CEPoll::add_usock(const int eid, const UDTSOCKET& u, const int* events)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    if (!events || (*events & UDT_EPOLL_IN))
        p->second->m_sUDTSocksIn.insert(u);
    if (!events || (*events & UDT_EPOLL_OUT))
        p->second->m_sUDTSocksOut.insert(u);

    return 0;
}

int CEPoll::add_ssock(const int eid, const SYSSOCKET& s, const int* events)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

#if __linux__
    epoll_event ev;
    memset(&ev, 0, sizeof(epoll_event));

    if (NULL == events)
        ev.events = EPOLLIN | EPOLLOUT;
    else
    {
        ev.events = 0;
        if (*events & UDT_EPOLL_IN)
            ev.events |= EPOLLIN;
        if (*events & UDT_EPOLL_OUT)
            ev.events |= EPOLLOUT;
    }

    ev.data.fd = s;
    if (::epoll_ctl(p->second->m_iLocalID, EPOLL_CTL_ADD, s, &ev) < 0)
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
    if (kevent(p->second->m_iLocalID, ev, evCount, NULL, 0, NULL) != 0)
        throw CUDTException();
#elif _WIN32
#endif

    int& eventMask = p->second->m_sLocals[s];
    eventMask |= *events;

    return 0;
}

int CEPoll::remove_usock(const int eid, const UDTSOCKET& u)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

    p->second->m_sUDTSocksIn.erase(u);
    p->second->m_sUDTSocksOut.erase(u);
    p->second->m_sUDTSocksEx.erase(u);

    return 0;
}

int CEPoll::remove_ssock(const int eid, const SYSSOCKET& s)
{
    CGuard pg(m_EPollLock);

    CEPollDescMap::iterator p = m_mPolls.find(eid);
    if (p == m_mPolls.end())
        throw CUDTException(5, 13);

#if __linux__
    epoll_event ev;  // ev is ignored, for compatibility with old Linux kernel only.
    if (::epoll_ctl(p->second->m_iLocalID, EPOLL_CTL_DEL, s, &ev) < 0)
        throw CUDTException();
#elif __APPLE__
    //not calling kevent with two events since if one of them is not being listened, whole kevent call fails
    struct kevent ev;
    EV_SET(&ev, s, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(p->second->m_iLocalID, &ev, 1, NULL, 0, NULL);
    EV_SET(&ev, s, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(p->second->m_iLocalID, &ev, 1, NULL, 0, NULL);  //ignoring return code, since event is removed in any case
#elif _WIN32
#endif

    p->second->m_sLocals.erase(s);

    return 0;
}

int CEPoll::wait(
    const int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds, int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    CEPollDesc* epollContext = nullptr;
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
            const int eventCount = doSystemPoll(epollContext, lrfds, lwfds);
            if (eventCount < 0)
                return -1;
            total += eventCount;
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

    CEPollDescMap::iterator i = m_mPolls.find(eid);
    if (i == m_mPolls.end())
        throw CUDTException(5, 13);

#if __linux__ || __APPLE__ 
    // release local/system epoll descriptor
    ::close(i->second->m_iLocalID);
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
                update_epoll_sets(uid, p->second->m_sUDTSocksIn, p->second->m_sUDTReads, enable);
            if ((events & UDT_EPOLL_OUT) != 0)
                update_epoll_sets(uid, p->second->m_sUDTSocksOut, p->second->m_sUDTWrites, enable);
            if ((events & UDT_EPOLL_ERR) != 0)
                update_epoll_sets(uid, p->second->m_sUDTSocksEx, p->second->m_sUDTExcepts, enable);
        }
    }
    lk.unlock();

    CTimer::triggerEvent();

    return 0;
}

void CEPoll::RemoveEPollEvent(UDTSOCKET socket)
{
    CGuard pg(m_EPollLock);
    CEPollDescMap::iterator p;
    for (p = m_mPolls.begin(); p != m_mPolls.end(); ++p)
    {
        p->second->m_sUDTReads.erase(socket);
        p->second->m_sUDTWrites.erase(socket);
        p->second->m_sUDTExcepts.erase(socket);
    }
}

#if __linux__ 
#   ifndef EPOLLRDHUP
#       define EPOLLRDHUP 0x2000 /* Android doesn't define EPOLLRDHUP, but it still works if defined properly. */
#   endif

int CEPoll::doSystemPoll(
    CEPollDesc* epollContext,
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds)
{
    const int max_events = 1024;
    epoll_event ev[max_events];
    int nfds = ::epoll_wait(epollContext->m_iLocalID, ev, max_events, 0);

    CGuard lk(m_EPollLock);
    int total = 0;
    for (int i = 0; i < nfds; ++i)
    {
        const bool hangup = (ev[i].events & (EPOLLHUP | EPOLLRDHUP)) > 0;

        if ((NULL != lrfds) && (ev[i].events & EPOLLIN))
        {
            lrfds->emplace((SYSSOCKET)ev[i].data.fd, (int)ev[i].events);
            ++total;
        }
        if ((NULL != lwfds) && (ev[i].events & EPOLLOUT))
        {
            //hangup - is an error when connecting, so adding error flag just for case
            lwfds->emplace(
                (SYSSOCKET)ev[i].data.fd,
                int(ev[i].events | (hangup ? UDT_EPOLL_ERR : 0)));
            ++total;
        }

        if (ev[i].events & (EPOLLIN | EPOLLOUT))
            continue;

        //event has not been returned yet
        if (lrfds != NULL &&
            epollContext->m_sUDTSocksIn.find(ev[i].data.fd) != epollContext->m_sUDTSocksIn.end())
        {
            lrfds->emplace((SYSSOCKET)ev[i].data.fd, (int)ev[i].events);
            ++total;
        }

        if (lwfds != NULL &&
            epollContext->m_sUDTSocksOut.find(ev[i].data.fd) != epollContext->m_sUDTSocksOut.end())
        {
            //hangup - is an error when connecting, so adding error flag just for case
            lwfds->emplace(
                (SYSSOCKET)ev[i].data.fd,
                int(ev[i].events | (hangup ? UDT_EPOLL_ERR : 0)));
            ++total;
        }
    }

    return total;
}

#elif __APPLE__

int CEPoll::doSystemPoll(
    CEPollDesc* epollContext,
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds)
{
    static const size_t MAX_EVENTS_TO_READ = 128;
    struct kevent ev[MAX_EVENTS_TO_READ];

    memset(ev, 0, sizeof(ev));
    struct timespec timeout;
    int msTimeOut = 0;
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
        const bool failure = (ev[i].flags & EV_ERROR) > 0;
        if ((NULL != lrfds) && (ev[i].filter == EVFILT_READ))
        {
            lrfds->emplace(
                ev[i].ident,
                UDT_EPOLL_IN | (failure ? UDT_EPOLL_ERR : 0));
        }
        if ((NULL != lwfds) && (ev[i].filter == EVFILT_WRITE))
        {
            lwfds->emplace(
                ev[i].ident,
                UDT_EPOLL_OUT | (failure ? UDT_EPOLL_ERR : 0));
        }
    }
    return nfds;
}

#elif _WIN32

int CEPoll::doSystemPoll(
    CEPollDesc* epollContextOriginal,
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds)
{
    CEPollDescWin32* epollContext = static_cast<CEPollDescWin32*>(epollContextOriginal);

    epollContext->readfds->fd_count = 0;
    epollContext->writefds->fd_count = 0;
    epollContext->exceptfds->fd_count = 0;

    reallocFdSetIfNeeded(
        epollContext->m_sLocals.size(),
        &epollContext->readfdsCapacity,
        &epollContext->readfds);
    reallocFdSetIfNeeded(
        epollContext->m_sLocals.size(),
        &epollContext->writefdsCapacity,
        &epollContext->writefds);
    reallocFdSetIfNeeded(
        epollContext->m_sLocals.size(),
        &epollContext->exceptfdsCapacity,
        &epollContext->exceptfds);

    for (const std::pair<SYSSOCKET, int>& fdAndEventMask: epollContext->m_sLocals)
    {
        if (lrfds && (fdAndEventMask.second & UDT_EPOLL_IN) > 0)
        {
            epollContext->readfds->fd_array[epollContext->readfds->fd_count++]
                = fdAndEventMask.first;
        }
        if (lwfds && (fdAndEventMask.second & UDT_EPOLL_OUT) > 0)
        {
            epollContext->writefds->fd_array[epollContext->writefds->fd_count++]
                = fdAndEventMask.first;
        }
        epollContext->exceptfds->fd_array[epollContext->exceptfds->fd_count++]
            = fdAndEventMask.first;
    }

    timeval tv;
    memset(&tv, 0, sizeof(tv));

    const int eventCount = ::select(
        0,
        epollContext->readfds->fd_count > 0
            ? reinterpret_cast<fd_set*>(epollContext->readfds)
            : NULL,
        epollContext->writefds->fd_count > 0
            ? reinterpret_cast<fd_set*>(epollContext->writefds)
            : NULL,
        epollContext->exceptfds->fd_count > 0
            ? reinterpret_cast<fd_set*>(epollContext->exceptfds)
            : NULL,
        &tv);
    if (eventCount < 0)
        return -1;
    //select sets fd_count to number of sockets triggered and 
        //moves those descriptors to the beginning of fd_array
    if (eventCount == 0)
        return eventCount;

    //using win32-specific select features to get O(1) here
    for (size_t i = 0; i < epollContext->readfds->fd_count; ++i)
        (*lrfds)[epollContext->readfds->fd_array[i]] = UDT_EPOLL_IN;
    for (size_t i = 0; i < epollContext->writefds->fd_count; ++i)
        (*lwfds)[epollContext->writefds->fd_array[i]] = UDT_EPOLL_OUT;
    for (size_t i = 0; i < epollContext->exceptfds->fd_count; ++i)
    {
        if (lrfds)
            (*lrfds)[epollContext->exceptfds->fd_array[i]] |= UDT_EPOLL_ERR;
        if (lwfds)
            (*lwfds)[epollContext->exceptfds->fd_array[i]] |= UDT_EPOLL_ERR;
    }

    return eventCount;
}

#else

#error "Missing CEPoll::doSystemPoll() implementation for the current platform"

#endif
