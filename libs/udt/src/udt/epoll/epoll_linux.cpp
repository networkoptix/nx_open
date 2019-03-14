#include "epoll_linux.h"

#ifdef __linux__

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#ifndef EPOLLRDHUP
#   define EPOLLRDHUP 0x2000 //< Android doesn't define EPOLLRDHUP, but it still works if defined properly.
#endif

EpollLinux::EpollLinux():
    m_epollFd(-1),
    m_interruptEventFd(-1)
{
    // Since Linux 2.6.8, the size argument is ignored, but must be greater than zero.
    m_epollFd = epoll_create(1024);
    if (m_epollFd < 0)
        throw CUDTException(-1, 0, errno);
    
    m_interruptEventFd = eventfd(0, EFD_NONBLOCK);
    if (m_interruptEventFd < 0)
    {
        ::close(m_epollFd);
        throw CUDTException(-1, 0, errno);
    }

    epoll_event _event;
    memset(&_event, 0, sizeof(_event));
    _event.data.fd = m_interruptEventFd;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_interruptEventFd, &_event) != 0)
    {
        ::close(m_epollFd);
        m_epollFd = -1;
        ::close(m_interruptEventFd);
        m_interruptEventFd = -1;
        throw CUDTException(-1, 0, errno);
    }
}

EpollLinux::~EpollLinux()
{
    ::close(m_epollFd);
    m_epollFd = -1;

    ::close(m_interruptEventFd);
    m_interruptEventFd = -1;
}

void EpollLinux::add(const SYSSOCKET& s, const int* events)
{
    epoll_event ev;
    memset(&ev, 0, sizeof(epoll_event));

    if (NULL == events)
    {
        ev.events = EPOLLIN | EPOLLOUT;
    }
    else
    {
        ev.events = 0;
        if (*events & UDT_EPOLL_IN)
            ev.events |= EPOLLIN;
        if (*events & UDT_EPOLL_OUT)
            ev.events |= EPOLLOUT;
    }

    ev.data.fd = s;
    if (::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, s, &ev) < 0)
        throw CUDTException();

    int& eventMask = m_sLocals[s];
    eventMask |= *events;
}

void EpollLinux::remove(const SYSSOCKET& s)
{
    epoll_event ev;  // ev is ignored, for compatibility with old Linux kernel only.
    if (::epoll_ctl(m_epollFd, EPOLL_CTL_DEL, s, &ev) < 0)
        throw CUDTException();

    m_sLocals.erase(s);
}

std::size_t EpollLinux::socketsPolledCount() const
{
    return m_sLocals.size();
}

int EpollLinux::poll(
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds,
    std::chrono::microseconds timeout)
{
    using namespace std::chrono;

    const int max_events = 1024;
    epoll_event ev[max_events];

    int systemTimeout = -1;
    const bool isTimeoutSpecified = timeout != microseconds::max();
    if (isTimeoutSpecified)
        systemTimeout = duration_cast<milliseconds>(timeout).count();

    const int nfds = ::epoll_wait(
        m_epollFd,
        ev,
        max_events,
        systemTimeout);

    int total = 0;
    for (int i = 0; i < nfds; ++i)
    {
        if (ev[i].data.fd == m_interruptEventFd)
        {
            uint64_t val = 0;
            read(m_interruptEventFd, &val, sizeof(val));
            continue;
        }

        const bool hangup = (ev[i].events & (EPOLLHUP | EPOLLRDHUP)) > 0;
        const bool failure = (ev[i].events & EPOLLERR) > 0;
        const int additionalOutputEventFlag = (hangup || failure) ? UDT_EPOLL_ERR : 0;

        if ((NULL != lrfds) && (ev[i].events & EPOLLIN))
        {
            lrfds->emplace(
                (SYSSOCKET)ev[i].data.fd,
                UDT_EPOLL_IN | additionalOutputEventFlag);
            ++total;
        }
        if ((NULL != lwfds) && (ev[i].events & EPOLLOUT))
        {
            //hangup - is an error when connecting, so adding error flag just for case
            lwfds->emplace(
                (SYSSOCKET)ev[i].data.fd,
                UDT_EPOLL_OUT | additionalOutputEventFlag);
            ++total;
        }
    }

    return total;
}

void EpollLinux::interrupt()
{
    uint64_t val = 1;
    write(m_interruptEventFd, &val, sizeof(val));
}

#endif // __linux__
