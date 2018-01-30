#include "epoll_macosx.h"

#include <iostream>

#ifdef __APPLE__

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

static const int USER_EVENT_IDENT = 11;

EpollMacosx::EpollMacosx():
    m_kqueueFd(-1)
{
    m_kqueueFd = kqueue();
    if (m_kqueueFd < 0)
        throw CUDTException(-1, 0, errno);

    //registering filter for interrupting poll
    struct kevent _newEvent;
    EV_SET(&_newEvent, USER_EVENT_IDENT, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);
    if (kevent(m_kqueueFd, &_newEvent, 1, NULL, 0, NULL) != 0)
    {
        ::close(m_kqueueFd);
        m_kqueueFd = -1;
        throw CUDTException(-1, 0, errno);
    }
}

EpollMacosx::~EpollMacosx()
{
    ::close(m_kqueueFd);
    m_kqueueFd = -1;
}

void EpollMacosx::add(const SYSSOCKET& s, const int* events)
{
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
    if (kevent(m_kqueueFd, ev, evCount, NULL, 0, NULL) != 0)
        throw CUDTException();

    int& eventMask = m_sLocals[s];
    eventMask |= *events;
}

void EpollMacosx::remove(const SYSSOCKET& s)
{
    //not calling kevent with two events since if one of them is not being listened, whole kevent call fails
    struct kevent ev;
    EV_SET(&ev, s, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(m_kqueueFd, &ev, 1, NULL, 0, NULL);
    EV_SET(&ev, s, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(m_kqueueFd, &ev, 1, NULL, 0, NULL);  //ignoring return code, since event is removed in any case

    m_sLocals.erase(s);
}

std::size_t EpollMacosx::socketsPolledCount() const
{
    return m_sLocals.size();
}

int EpollMacosx::poll(
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds,
    std::chrono::microseconds timeout)
{
    using namespace std::chrono;

    static const size_t MAX_EVENTS_TO_READ = 128;
    struct kevent ev[MAX_EVENTS_TO_READ];
    memset(ev, 0, sizeof(ev));

    const bool isTimeoutSpecified = timeout != microseconds::max();
    struct timespec systemTimeout;
    memset(&systemTimeout, 0, sizeof(systemTimeout));
    if (isTimeoutSpecified)
    {
        const auto fullSeconds = duration_cast<seconds>(timeout);
        systemTimeout.tv_sec = fullSeconds.count();
        systemTimeout.tv_nsec = duration_cast<nanoseconds>(timeout - fullSeconds).count();
    }

    const int nfds = kevent(
        m_kqueueFd,
        NULL,
        0,
        ev,
        MAX_EVENTS_TO_READ,
        isTimeoutSpecified ? &systemTimeout : NULL);

    int result = nfds;
    for (int i = 0; i < nfds; ++i)
    {
        if (ev[i].filter == EVFILT_USER)
        {
            --result;
            continue;
        }

        const bool failure = (ev[i].flags & (EV_ERROR | EV_EOF)) > 0;
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

    return result;
}

void EpollMacosx::interrupt()
{
    struct kevent _newEvent;
    EV_SET(&_newEvent, USER_EVENT_IDENT, EVFILT_USER, EV_CLEAR, NOTE_TRIGGER, 0, NULL);
    kevent(m_kqueueFd, &_newEvent, 1, NULL, 0, NULL);
}

#endif // __APPLE__
