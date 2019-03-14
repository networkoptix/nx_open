#include <qglobal.h>

#include "pollset.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <map>

#include "../system_socket.h"

#ifdef Q_OS_ANDROID
#   define EPOLLRDHUP 0x2000 /* Android doesn't define EPOLLRDHUP, but it still works if defined properly. */
#endif

using namespace std;

namespace nx {
namespace network {
namespace aio {

//-------------------------------------------------------------------------------------------------
// PollSetImpl.

class PollSetImpl
{
public:
    struct SockData
    {
        /** Bit 'or' of listened events (EPOLLIN, EPOLLOUT, etc..). */
        int eventsMask;
        bool markedForRemoval;
        std::array<void*, aio::etMax> eventTypeToUserData;

        SockData():
            eventsMask(0),
            markedForRemoval(false)
        {
            eventTypeToUserData.fill(nullptr);
        }

        SockData(
            int _eventsMask,
            bool _markedForRemoval)
            :
            eventsMask(_eventsMask),
            markedForRemoval(_markedForRemoval)
        {
            eventTypeToUserData.fill(nullptr);
        }
    };

    /** map<fd, pair<events mask, user data>>. */
    using MonitoredEventMap = std::map<Pollable*, SockData>;

    int epollSetFd;
    MonitoredEventMap monitoredEvents;
    int signalledSockCount;
    size_t epollEventsArrayCapacity;
    epoll_event* epollEventsArray;
    /** Linux event, used to interrupt epoll_wait. */
    int eventFd;

    PollSetImpl():
        epollSetFd(-1),
        signalledSockCount(0),
        epollEventsArrayCapacity(32),
        epollEventsArray(new epoll_event[epollEventsArrayCapacity]),
        eventFd(-1)
    {
    }

    ~PollSetImpl()
    {
        delete[] epollEventsArray;
        epollEventsArray = NULL;
        epollEventsArrayCapacity = 0;
    }
};

//-------------------------------------------------------------------------------------------------
// ConstIteratorImplOld.

class ConstIteratorImplOld
{
public:
    int currentIndex;
    PollSetImpl* pollSetImpl;
    aio::EventType triggeredEvent;
    aio::EventType handlerToUse;

    ConstIteratorImplOld():
        currentIndex(0),
        pollSetImpl(NULL),
        triggeredEvent(aio::etNone),
        handlerToUse(aio::etNone)
    {
    }

    void selectNextSocket()
    {
        for (; currentIndex < pollSetImpl->signalledSockCount; )
        {
            PollSetImpl::MonitoredEventMap::const_pointer socketData = NULL;
            if (currentIndex >= 0)
            {
                socketData = static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(
                    pollSetImpl->epollEventsArray[currentIndex].data.ptr);
                // Reporting EPOLLIN and EPOLLOUT as different events to allow
                // them to have different handlers.
                if (socketData != NULL &&
                    !socketData->second.markedForRemoval &&
                    handlerToUse == aio::etRead &&
                    (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLOUT))
                {
                    // If both EPOLLIN and EPOLLOUT have occured and EPOLLIN
                    // has already been reported then reporting EPOLLOUT.
                    handlerToUse = aio::etWrite;
                    triggeredEvent = (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLERR)
                        ? aio::etError
                        : aio::etWrite;
                    return;
                }
            }

            ++currentIndex;
            if (currentIndex >= pollSetImpl->signalledSockCount)
                return; //< Reached end, iterator is invalid.

            if (pollSetImpl->epollEventsArray[currentIndex].data.ptr == NULL)
            {
                // Reading eventfd and selecting next socket.
                uint64_t val = 0;
                if (read(pollSetImpl->eventFd, &val, sizeof(val)) == -1)
                {
                    // TODO: #ak Can this really happen?
                }
                continue;
            }

            socketData = static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(
                pollSetImpl->epollEventsArray[currentIndex].data.ptr);
            if (socketData->second.markedForRemoval)
                continue; //< Skipping socket.

            // Processing all triggered events one by one.
            if (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLERR)
            {
                // Reporting aio::etError to every one who listens.
                pollSetImpl->epollEventsArray[currentIndex].events |=
                    socketData->second.eventsMask & (EPOLLIN | EPOLLOUT);
                handlerToUse = (socketData->second.eventsMask & EPOLLIN) ? aio::etRead : aio::etWrite;
                triggeredEvent = aio::etError;
            }
            else
            {
                if (pollSetImpl->epollEventsArray[currentIndex].events & (EPOLLHUP | EPOLLRDHUP))
                {
                    // Reporting events to every one who listens.
                    pollSetImpl->epollEventsArray[currentIndex].events |=
                        socketData->second.eventsMask & (EPOLLIN | EPOLLOUT);
                    handlerToUse = (socketData->second.eventsMask & EPOLLIN) ? aio::etRead : aio::etWrite;
                    // Reporting connection closure as an error when writing data to
                    // provide behavior similar to recv/send functions.
                    // recv returns (ok, 0 bytes) in case of a closed connection.
                    // send reports error in that case.
                    if (socketData->second.eventsMask & EPOLLOUT)
                        triggeredEvent = aio::etError;
                    else
                        triggeredEvent = handlerToUse;
                }
                else if (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLIN)
                {
                    handlerToUse = aio::etRead;
                    triggeredEvent = aio::etRead;
                }
                else if (pollSetImpl->epollEventsArray[currentIndex].events & EPOLLOUT)
                {
                    handlerToUse = aio::etWrite;
                    triggeredEvent = aio::etWrite;
                }
            }
            return;
        }
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
    m_impl->selectNextSocket();
    return *this;
}

const Pollable* PollSet::const_iterator::socket() const
{
    return static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(
        m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr)->first;
}

Pollable* PollSet::const_iterator::socket()
{
    return static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(
        m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr)->first;
}

aio::EventType PollSet::const_iterator::eventType() const
{
    return m_impl->triggeredEvent;
}

void* PollSet::const_iterator::userData()
{
    return static_cast<PollSetImpl::MonitoredEventMap::pointer>(
        m_impl->pollSetImpl->epollEventsArray[m_impl->currentIndex].data.ptr
        )->second.eventTypeToUserData[m_impl->handlerToUse];
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

PollSet::PollSet():
    m_impl(new PollSetImpl())
{
    m_impl->epollSetFd = epoll_create(256);
    m_impl->eventFd = eventfd(0, EFD_NONBLOCK);

    if (m_impl->epollSetFd > 0 && m_impl->eventFd > 0)
    {
        epoll_event _event;
        memset(&_event, 0, sizeof(_event));
        _event.data.ptr = NULL; //m_impl->eventFd;
        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if (epoll_ctl(m_impl->epollSetFd, EPOLL_CTL_ADD, m_impl->eventFd, &_event))
        {
            close(m_impl->eventFd);
            m_impl->eventFd = -1;
        }
    }
}

PollSet::~PollSet()
{
    if (m_impl->epollSetFd > 0)
    {
        close(m_impl->epollSetFd);
        m_impl->epollSetFd = -1;
    }
    if (m_impl->eventFd > 0)
    {
        close(m_impl->eventFd);
        m_impl->eventFd = -1;
    }

    delete m_impl;
    m_impl = NULL;
}

bool PollSet::isValid() const
{
    return (m_impl->epollSetFd > 0) && (m_impl->eventFd > 0);
}

void PollSet::interrupt()
{
    uint64_t val = 1;
    if (write(m_impl->eventFd, &val, sizeof(val)) != sizeof(val))
    {
        //TODO #ak: can this really occur ???
    }
}

bool PollSet::add(Pollable* const sock, EventType eventType, void* userData)
{
    const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;

    pair<PollSetImpl::MonitoredEventMap::iterator, bool> p =
        m_impl->monitoredEvents.insert(make_pair(sock, PollSetImpl::SockData(0, false)));
    if (p.second)
    {
        // Adding new fd to set.
        epoll_event _event;
        memset(&_event, 0, sizeof(_event));
        _event.data.ptr = &*p.first;
        _event.events = epollEventType | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if (epoll_ctl(m_impl->epollSetFd, EPOLL_CTL_ADD, sock->handle(), &_event) == 0)
        {
            p.first->second.eventsMask = epollEventType;
            p.first->second.eventTypeToUserData[eventType] = userData;
            return true;
        }
        else
        {
            m_impl->monitoredEvents.erase(p.first);
            return false;
        }
    }
    else
    {
        if (p.first->second.eventsMask & epollEventType)
            return true; //< Event evenType is already listened on socket.

        epoll_event _event;
        memset(&_event, 0, sizeof(_event));
        _event.data.ptr = &*p.first;
        _event.events = epollEventType | p.first->second.eventsMask | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        if (epoll_ctl(m_impl->epollSetFd, EPOLL_CTL_MOD, sock->handle(), &_event) == 0)
        {
            p.first->second.eventsMask |= epollEventType;
            p.first->second.eventTypeToUserData[eventType] = userData;
            return true;
        }
        else
        {
            return false;
        }
    }
}

void PollSet::remove(Pollable* const sock, EventType eventType)
{
    const int epollEventType = eventType == etRead ? EPOLLIN : EPOLLOUT;
    PollSetImpl::MonitoredEventMap::iterator it = m_impl->monitoredEvents.find(sock);
    if ((it == m_impl->monitoredEvents.end()) || !(it->second.eventsMask & epollEventType))
        return;

    const int eventsBesidesRemovedOne = (it->second.eventsMask & (EPOLLIN | EPOLLOUT)) & (~epollEventType);
    if (eventsBesidesRemovedOne)
    {
        // Socket is being listened for multiple events, modifying event set.
        epoll_event _event;
        memset(&_event, 0, sizeof(_event));
        _event.data.ptr = &*it;
        _event.events = eventsBesidesRemovedOne | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
        epoll_ctl(m_impl->epollSetFd, EPOLL_CTL_MOD, sock->handle(), &_event);
        it->second.eventsMask &= ~epollEventType; // Resetting disabled event bit.
                                                  // TODO: #ak Better remove following linear run.
        for (int i = 0; i < m_impl->signalledSockCount; ++i)
        {
            if (m_impl->epollEventsArray[i].data.ptr != NULL &&
                static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(
                    m_impl->epollEventsArray[i].data.ptr)->first == sock)
            {
                m_impl->epollEventsArray[i].events &= ~epollEventType; //< Ignoring event which has just been removed.
                break;
            }
        }
        it->second.eventTypeToUserData[eventType] = NULL;
    }
    else
    {
        // Socket is being listened for epollEventType only, removing socket...
        epoll_ctl(m_impl->epollSetFd, EPOLL_CTL_DEL, sock->handle(), NULL);
        it->second.markedForRemoval = true;
        // TODO: #ak Netter remove following linear run.
        for (int i = 0; i < m_impl->signalledSockCount; ++i)
        {
            if (m_impl->epollEventsArray[i].data.ptr != NULL &&
                static_cast<PollSetImpl::MonitoredEventMap::const_pointer>(m_impl->epollEventsArray[i].data.ptr)->first == sock)
            {
                m_impl->epollEventsArray[i].data.ptr = NULL;
                break;
            }
        }
        m_impl->monitoredEvents.erase(it);
    }
}

size_t PollSet::size() const
{
    return m_impl->monitoredEvents.size();
}

int PollSet::poll(int millisToWait)
{
    int result = epoll_wait(
        m_impl->epollSetFd,
        m_impl->epollEventsArray,
        m_impl->epollEventsArrayCapacity,
        millisToWait < 0 ? -1 : millisToWait);
    m_impl->signalledSockCount = result < 0 ? 0 : result;
    return result;
}

PollSet::const_iterator PollSet::begin() const
{
    const_iterator _begin;
    _begin.m_impl->currentIndex = -1;
    _begin.m_impl->pollSetImpl = m_impl;
    _begin.m_impl->selectNextSocket();
    return _begin;
}

PollSet::const_iterator PollSet::end() const
{
    const_iterator _end;
    _end.m_impl->currentIndex = m_impl->signalledSockCount;
    _end.m_impl->pollSetImpl = m_impl;
    return _end;
}

} // namespace aio
} // namespace network
} // namespace nx
