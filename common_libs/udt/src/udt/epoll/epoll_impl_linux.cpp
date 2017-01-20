#include "epoll_impl_linux.h"

#ifdef __linux__

#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>

#ifndef EPOLLRDHUP
#   define EPOLLRDHUP 0x2000 /* Android doesn't define EPOLLRDHUP, but it still works if defined properly. */
#endif

CEPollDescLinux::CEPollDescLinux()
{
    m_iLocalID = epoll_create(1024);    //Since Linux 2.6.8, the size argument is ignored, but must be greater than zero
    if (m_iLocalID < 0)
        throw CUDTException(-1, 0, errno);
}

CEPollDescLinux::~CEPollDescLinux()
{
    ::close(m_iLocalID);
}

void CEPollDescLinux::add(const SYSSOCKET& s, const int* events)
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
    if (::epoll_ctl(m_iLocalID, EPOLL_CTL_ADD, s, &ev) < 0)
        throw CUDTException();

    int& eventMask = m_sLocals[s];
    eventMask |= *events;
}

void CEPollDescLinux::remove(const SYSSOCKET& s)
{
    epoll_event ev;  // ev is ignored, for compatibility with old Linux kernel only.
    if (::epoll_ctl(m_iLocalID, EPOLL_CTL_DEL, s, &ev) < 0)
        throw CUDTException();

    m_sLocals.erase(s);
}

int CEPollDescLinux::doSystemPoll(
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds)
{
    const int max_events = 1024;
    epoll_event ev[max_events];
    int nfds = ::epoll_wait(m_iLocalID, ev, max_events, 0);

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
    }

    return total;
}

#endif // __linux__
