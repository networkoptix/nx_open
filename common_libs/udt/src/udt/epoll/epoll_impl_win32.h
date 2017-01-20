#pragma once

#include "epoll_impl.h"

#ifdef _WIN32

constexpr static const size_t kInitialFdSetSize = 1024;
constexpr static const size_t kFdSetIncreaseStep = 1024;

typedef struct CustomFdSet
{
    u_int fd_count;               /* how many are SET? */
                                  //!an array of SOCKETs. Actually, this array has size \a PollSetImpl::fdSetSize
    SOCKET fd_array[kInitialFdSetSize];
} CustomFdSet;

class CEPollDescWin32:
    public EpollImpl
{
public:
    CEPollDescWin32();
    virtual ~CEPollDescWin32() override;

    virtual void add(const SYSSOCKET& s, const int* events) override;
    virtual void remove(const SYSSOCKET& s) override;
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds) override;

private:
    CustomFdSet* m_readfds;
    size_t m_readfdsCapacity;
    CustomFdSet* m_writefds;
    size_t m_writefdsCapacity;
    CustomFdSet* m_exceptfds;
    size_t m_exceptfdsCapacity;

    CEPollDescWin32(const CEPollDescWin32&) = delete;
    CEPollDescWin32& operator=(const CEPollDescWin32&) = delete;
};

#endif // _WIN32
