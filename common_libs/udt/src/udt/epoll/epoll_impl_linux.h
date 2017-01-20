#pragma once

#include "epoll_impl.h"

#ifdef __linux__

class CEPollDescLinux:
    public EpollImpl
{
public:
    CEPollDescLinux();
    virtual ~CEPollDescLinux() override;
    
    virtual void add(const SYSSOCKET& s, const int* events) override;
    virtual void remove(const SYSSOCKET& s) override;
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds) override;
};

#endif // __linux__
