#pragma once

#include "abstract_epoll.h"

#ifdef __APPLE__

class CEPollDescMacosx:
    public AbstractEpoll
{
public:
    CEPollDescMacosx();
    virtual ~CEPollDescMacosx() override;

    virtual void add(const SYSSOCKET& s, const int* events) override;
    virtual void remove(const SYSSOCKET& s) override;
    virtual std::size_t socketsPolledCount() const override;
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        std::chrono::microseconds timeout) override;

private:
    int m_kqueueFd;
    /** map<local (non-UDT) descriptor, event mask (UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR)>. */
    std::map<SYSSOCKET, int> m_sLocals;
};

#endif // __APPLE__
