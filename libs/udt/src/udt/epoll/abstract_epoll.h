#pragma once

#include <chrono>
#include <map>

#include "../result.h"
#include "../udt.h"

class AbstractEpoll
{
public:
    virtual ~AbstractEpoll() = default;

    virtual Result<> initialize() = 0;

    virtual Result<> add(const SYSSOCKET& s, const int* events) = 0;
    virtual void remove(const SYSSOCKET& s) = 0;
    virtual std::size_t socketsPolledCount() const = 0;

    /**
     * @param lrfds Sockets, available for reading. map<{socket handle}, {bitmap of UDT_EPOLL_* values}>.
     * @param lwfds Sockets, available for writing. map<{socket handle}, {bitmap of UDT_EPOLL_* values}>.
     * @param std::chrono::microseconds::max() means no timeout.
     * @return Number of events triggered. 0 in case of timeout.
     */
    virtual Result<int> poll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        std::chrono::microseconds timeout) = 0;

    /**
     * Causes AbstractEpoll::poll running in another thread to return immediately with result 0.
     * If AbstractEpoll::poll called after this method it will return 0 immediately too.
     */
    virtual void interrupt() = 0;
};
