#include "epoll_impl_macosx.h"

#ifdef __APPLE__

#include <unistd.h>
#include <sys/event.h>

CEPollDescMacosx::CEPollDescMacosx()
{
    m_iLocalID = kqueue();
    if (m_iLocalID < 0)
        throw CUDTException(-1, 0, errno);
}

CEPollDescMacosx::~CEPollDescMacosx()
{
    ::close(m_iLocalID);
}

void CEPollDescMacosx::add(const SYSSOCKET& s, const int* events)
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
    if (kevent(m_iLocalID, ev, evCount, NULL, 0, NULL) != 0)
        throw CUDTException();

    int& eventMask = m_sLocals[s];
    eventMask |= *events;
}

void CEPollDescMacosx::remove(const SYSSOCKET& s)
{
    //not calling kevent with two events since if one of them is not being listened, whole kevent call fails
    struct kevent ev;
    EV_SET(&ev, s, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(m_iLocalID, &ev, 1, NULL, 0, NULL);
    EV_SET(&ev, s, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(m_iLocalID, &ev, 1, NULL, 0, NULL);  //ignoring return code, since event is removed in any case

    m_sLocals.erase(s);
}

int CEPollDescMacosx::doSystemPoll(
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
        m_iLocalID,
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

#endif // __APPLE__
