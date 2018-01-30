#if defined(__APPLE__) || defined(__FreeBSD__)

#include "pollset.h"

#include <limits>
#include <set>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include "../system_socket.h"

//TODO #ak get rid of PollSetImpl::monitoredEvents due to performance cosiderations

using namespace std;

namespace nx {
namespace network {
namespace aio {

static const int kMaxEventsToReceive = 32;

class PollSetImpl
{
public:
    /** map<pair<socket, event type> > */
    using MonitoredEventSet = std::set<std::pair<Pollable*, aio::EventType>>;

    int kqueueFd;
    MonitoredEventSet monitoredEvents;
    int receivedEventCount;
    struct kevent receivedEventlist[kMaxEventsToReceive];
    /** Event, used to interrupt epoll_wait. */
    int eventFd;

    PollSetImpl():
        kqueueFd(-1),
        receivedEventCount(0),
        eventFd(-1)
    {
        memset(&receivedEventlist, 0, sizeof(receivedEventlist));
    }
};

//-------------------------------------------------------------------------------------------------
// ConstIteratorImplOld.

class ConstIteratorImplOld
{
public:
    PollSetImpl* pollSetImpl = nullptr;
    int currentIndex = 0;

    ConstIteratorImplOld() = default;

    ConstIteratorImplOld(const ConstIteratorImplOld& right):
        pollSetImpl(right.pollSetImpl),
        currentIndex(right.currentIndex)
    {
    }

    const ConstIteratorImplOld& operator=(const ConstIteratorImplOld& right)
    {
        pollSetImpl = right.pollSetImpl;
        currentIndex = right.currentIndex;
        return *this;
    }
};

//-------------------------------------------------------------------------------------------------
// PollSet::const_iterator.

PollSet::const_iterator::const_iterator():
    m_impl(new ConstIteratorImplOld())
{
}

PollSet::const_iterator::const_iterator(const const_iterator& right):
    m_impl(new ConstIteratorImplOld(*right.m_impl))
{
}

PollSet::const_iterator::~const_iterator()
{
    delete m_impl;
    m_impl = NULL;
}

PollSet::const_iterator& PollSet::const_iterator::operator=(const const_iterator& right)
{
    *m_impl = *right.m_impl;
    return *this;
}

PollSet::const_iterator PollSet::const_iterator::operator++(int) //< it++
{
    const_iterator bak(*this);
    ++(*this);
    return bak;
}

PollSet::const_iterator& PollSet::const_iterator::operator++() //< ++it
{
    m_impl->currentIndex++;
    while (m_impl->currentIndex < m_impl->pollSetImpl->receivedEventCount &&
        (m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].filter == EVFILT_USER ||
            m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].ident == (uintptr_t)-1))  //socket has been removed
    {
        m_impl->currentIndex++;
    }
    return *this;
}

Pollable* PollSet::const_iterator::socket()
{
    return static_cast<Pollable*>(
        m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].udata);
}

const Pollable* PollSet::const_iterator::socket() const
{
    return static_cast<Pollable*>(
        m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].udata);
}

aio::EventType PollSet::const_iterator::eventType() const
{
    const int kFilterType = m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].filter;
    aio::EventType revents = aio::etNone;
    if (m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].flags & EV_ERROR)
        revents = aio::etError;
    else if (kFilterType == EVFILT_READ)
        revents = aio::etRead;
    else if (kFilterType == EVFILT_WRITE)
        revents = aio::etWrite;
    return revents;
}

void* PollSet::const_iterator::userData()
{
    return
        static_cast<Pollable*>(m_impl->pollSetImpl->receivedEventlist[m_impl->currentIndex].udata)
        ->impl()->monitoredEvents[eventType()].userData;
}

bool PollSet::const_iterator::operator==(const const_iterator& right) const
{
    return (m_impl->pollSetImpl == right.m_impl->pollSetImpl)
        && (m_impl->currentIndex == right.m_impl->currentIndex);
}

bool PollSet::const_iterator::operator!=(const const_iterator& right) const
{
    return !(*this == right);
}

//-------------------------------------------------------------------------------------------------
// PollSet.

static const int kUserEventIdent = 11;

PollSet::PollSet():
    m_impl(new PollSetImpl())
{
    m_impl->kqueueFd = kqueue();
    if (m_impl->kqueueFd < 0)
        return;

    // Registering filter for interrupting poll.
    struct kevent _newEvent;
    EV_SET(&_newEvent, kUserEventIdent, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);
    kevent(m_impl->kqueueFd, &_newEvent, 1, NULL, 0, NULL);
}

PollSet::~PollSet()
{
    if (m_impl->kqueueFd > 0)
    {
        close(m_impl->kqueueFd);
        m_impl->kqueueFd = -1;
    }
    delete m_impl;
    m_impl = NULL;
}

bool PollSet::isValid() const
{
    return m_impl->kqueueFd > 0;
}

void PollSet::interrupt()
{
    struct kevent _newEvent;
    EV_SET(&_newEvent, kUserEventIdent, EVFILT_USER, EV_CLEAR, NOTE_TRIGGER, 0, NULL);
    kevent(m_impl->kqueueFd, &_newEvent, 1, NULL, 0, NULL);
}

bool PollSet::add(Pollable* const sock, EventType eventType, void* userData)
{
    pair<PollSetImpl::MonitoredEventSet::iterator, bool> p =
        m_impl->monitoredEvents.insert(make_pair(sock, eventType));
    if (!p.second)
        return true; //< Event evenType is already listened on socket.

    const short kfilterType = eventType == etRead ? EVFILT_READ : EVFILT_WRITE;

    // Adding new fd to set.
    struct kevent _newEvent;
    EV_SET(&_newEvent, sock->handle(), kfilterType, EV_ADD | EV_ENABLE, 0, 0, sock);
    if (kevent(m_impl->kqueueFd, &_newEvent, 1, NULL, 0, NULL) == 0)
    {
        sock->impl()->monitoredEvents[eventType].userData = userData;
        return true;
    }

    m_impl->monitoredEvents.erase(p.first);
    return false;
}

void PollSet::remove(Pollable* const sock, EventType eventType)
{
    PollSetImpl::MonitoredEventSet::iterator it =
        m_impl->monitoredEvents.find(make_pair(sock, eventType));
    if (it == m_impl->monitoredEvents.end())
        return;

    const int kfilterType = eventType == etRead ? EVFILT_READ : EVFILT_WRITE;

    struct kevent _eventChange;
    EV_SET(&_eventChange, sock->handle(), kfilterType, EV_DELETE, 0, 0, NULL);
    kevent(m_impl->kqueueFd, &_eventChange, 1, NULL, 0, NULL); //< Ignoring return code, since event is removed in any case.
    m_impl->monitoredEvents.erase(it);

    // Erasing socket from occured events array.
    // TODO: #ak Better remove this linear run.
    for (int i = 0; i < m_impl->receivedEventCount; ++i)
    {
        if ((AbstractSocket::SOCKET_HANDLE)m_impl->receivedEventlist[i].ident == sock->handle() &&
            m_impl->receivedEventlist[i].filter == kfilterType)
        {
            m_impl->receivedEventlist[i].ident = (uintptr_t)-1;
        }
    }

    // TODO: #ak Must not save user data in socket.
    sock->impl()->monitoredEvents[eventType].userData = NULL;
}

size_t PollSet::size() const
{
    return m_impl->monitoredEvents.size();
}

static const int kMillisInSec = 1000;
static const int kNsecInMs = 1000000;

int PollSet::poll(int millisToWait)
{
    memset(&m_impl->receivedEventlist, 0, sizeof(m_impl->receivedEventlist));
    struct timespec timeout;
    if (millisToWait >= 0)
    {
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = millisToWait / kMillisInSec;
        timeout.tv_nsec = (millisToWait % kMillisInSec) * kNsecInMs;
    }
    m_impl->receivedEventCount = 0;

    int result = kevent(
        m_impl->kqueueFd,
        NULL,
        0,
        m_impl->receivedEventlist,
        sizeof(m_impl->receivedEventlist) / sizeof(*m_impl->receivedEventlist),
        millisToWait >= 0 ? &timeout : NULL);

    if (result == 0) //< Timed out.
        return 0;
    if (result < 0)
        return errno == EINTR ? 0 : result; //< Not reporting EINTR as an error since it is quite normal.

    m_impl->receivedEventCount = result;
    // Not reporting event used to interrupt blocking poll.
    for (int i = 0; i < result; ++i)
    {
        if (m_impl->receivedEventlist[i].filter == EVFILT_USER)
            --result;
    }

    return result;
}

PollSet::const_iterator PollSet::begin() const
{
    const_iterator beginIter;
    beginIter.m_impl->pollSetImpl = m_impl;
    beginIter.m_impl->currentIndex = -1;
    ++beginIter; //< Moving to the first element (it may have non - 0 index in case of USER_EVENT).
    return beginIter;
}

PollSet::const_iterator PollSet::end() const
{
    const_iterator endIter;
    endIter.m_impl->pollSetImpl = m_impl;
    endIter.m_impl->currentIndex = m_impl->receivedEventCount;
    return endIter;
}

} // namespace aio
} // namespace network
} // namespace nx

#endif  //defined(__APPLE__) || defined(__FreeBSD__)
