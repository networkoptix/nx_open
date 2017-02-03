#pragma once

#include "abstract_epoll.h"

#ifdef _WIN32

constexpr static const size_t kInitialFdSetSize = 1024;
constexpr static const size_t kFdSetIncreaseStep = 1024;

typedef struct CustomFdSet
{
    u_int fd_count;               /* how many are SET? */
                                  //!an array of SOCKETs. Actually, this array has size \a PollSetImpl::fdSetSize
    SOCKET fd_array[kInitialFdSetSize];
} CustomFdSet;

class EpollWin32:
    public AbstractEpoll
{
public:
    EpollWin32();
    virtual ~EpollWin32() override;

    virtual void add(const SYSSOCKET& s, const int* events) override;
    virtual void remove(const SYSSOCKET& s) override;
    virtual std::size_t socketsPolledCount() const override;
    virtual int poll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        std::chrono::microseconds timeout) override;
    virtual void interrupt() override;

private:
    /** map<local (non-UDT) descriptor, event mask (UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR)>. */
    std::map<SYSSOCKET, int> m_sLocals;
    SYSSOCKET m_interruptionSocket;
    sockaddr_in m_interruptionSocketLocalAddress;

    CustomFdSet* m_readfds;
    size_t m_readfdsCapacity;
    CustomFdSet* m_writefds;
    size_t m_writefdsCapacity;
    CustomFdSet* m_exceptfds;
    size_t m_exceptfdsCapacity;

    void prepareForPolling(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds);
    void prepareOutEvents(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        bool* receivedInterruptEvent);

    void initializeInterruptSocket();
    void freeInterruptSocket();

    EpollWin32(const EpollWin32&) = delete;
    EpollWin32& operator=(const EpollWin32&) = delete;
};

#endif // _WIN32
