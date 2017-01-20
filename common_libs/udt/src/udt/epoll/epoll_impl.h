#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "../udt.h"

class EpollImpl
{
public:
    EpollImpl();
    virtual ~EpollImpl() = default;

    int addUdtSocket(const UDTSOCKET& u, const int* events);
    int removeUdtSocket(const UDTSOCKET& u);
    void removeUdtSocketEvents(const UDTSOCKET& socket);

    virtual void add(const SYSSOCKET& s, const int* events) = 0;
    virtual void remove(const SYSSOCKET& s) = 0;

    int wait(
        std::map<UDTSOCKET, int>* udtReadFds, std::map<UDTSOCKET, int>* udtWriteFds,
        int64_t msTimeout,
        std::map<SYSSOCKET, int>* systemReadFds, std::map<SYSSOCKET, int>* systemWriteFds);

    void updateEpollSets(int events, const UDTSOCKET& socketId, bool enable);

protected:
    /** map<local (non-UDT) descriptor, event mask (UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR)>. */
    std::map<SYSSOCKET, int> m_sLocals;

    /**
     * @param std::chrono::microseconds::max() means no timeout.
     * @return Number of events triggered. -1 in case of error. 0 in case of timeout expiration.
     */
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds,
        std::chrono::microseconds timeout) = 0;

private:
    std::mutex m_mutex;

    std::set<UDTSOCKET> m_sUDTSocksOut;       // set of UDT sockets waiting for write events
    std::set<UDTSOCKET> m_sUDTSocksIn;        // set of UDT sockets waiting for read events
    std::set<UDTSOCKET> m_sUDTSocksEx;        // set of UDT sockets waiting for exceptions

    std::set<UDTSOCKET> m_sUDTWrites;         // UDT sockets ready for write
    std::set<UDTSOCKET> m_sUDTReads;          // UDT sockets ready for read
    std::set<UDTSOCKET> m_sUDTExcepts;        // UDT sockets with exceptions (connection broken, etc.)

    bool updateEpollSets(
        const UDTSOCKET& uid,
        const std::set<UDTSOCKET>& watch,
        std::set<UDTSOCKET>& result,
        bool enable);
    int addUdtSocketEvents(
        std::map<UDTSOCKET, int>* udtReadFds,
        std::map<UDTSOCKET, int>* udtWriteFds);
};
