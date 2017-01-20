#pragma once

#include "epoll_impl.h"

#ifdef __APPLE__

class CEPollDescMacosx:
    public EpollImpl
{
public:
    CEPollDescMacosx();
    virtual ~CEPollDescMacosx() override;

    virtual void add(const SYSSOCKET& s, const int* events) override;
    virtual void remove(const SYSSOCKET& s) override;
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        std::chrono::microseconds timeout) override;

private:
    int m_kqueueFd;
};

#endif // __APPLE__
