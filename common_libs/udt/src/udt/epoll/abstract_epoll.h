#pragma once

#include <chrono>
#include <map>

#include "../udt.h"

class AbstractEpoll
{
public:
    virtual ~AbstractEpoll() = default;

    virtual void add(const SYSSOCKET& s, const int* events) = 0;
    virtual void remove(const SYSSOCKET& s) = 0;
    virtual std::size_t socketsPolledCount() const = 0;
    /**
     * @param std::chrono::microseconds::max() means no timeout.
     * @return Number of events triggered. -1 in case of error. 0 in case of timeout expiration.
     */
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        std::chrono::microseconds timeout) = 0;
};
