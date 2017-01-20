#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "../udt.h"

class EpollImpl
{
public:
    std::set<UDTSOCKET> m_sUDTSocksOut;       // set of UDT sockets waiting for write events
    std::set<UDTSOCKET> m_sUDTSocksIn;        // set of UDT sockets waiting for read events
    std::set<UDTSOCKET> m_sUDTSocksEx;        // set of UDT sockets waiting for exceptions

    std::map<SYSSOCKET, int> m_sLocals;       // map<local (non-UDT) descriptor, event mask (UDT_EPOLL_IN | UDT_EPOLL_OUT | UDT_EPOLL_ERR)>

    std::set<UDTSOCKET> m_sUDTWrites;         // UDT sockets ready for write
    std::set<UDTSOCKET> m_sUDTReads;          // UDT sockets ready for read
    std::set<UDTSOCKET> m_sUDTExcepts;        // UDT sockets with exceptions (connection broken, etc.)

    EpollImpl();
    virtual ~EpollImpl() = default;

    int add_usock(const UDTSOCKET& u, const int* events);
    int remove_usock(const UDTSOCKET& u);
    void removeSocketEvents(const UDTSOCKET& socket);

    virtual void add(const SYSSOCKET& s, const int* events) = 0;
    virtual void remove(const SYSSOCKET& s) = 0;
    /**
     * @return Number of events triggered. -1 in case of error.
     */
    virtual int doSystemPoll(
        std::map<SYSSOCKET, int>* lrfds,
        std::map<SYSSOCKET, int>* lwfds) = 0;

    void updateEpollSets(int events, const UDTSOCKET& socketId, bool enable);

private:
    /**
     * Local system epoll ID.
     */
    int m_iLocalID;
    std::mutex m_mutex;

    bool updateEpollSets(
        const UDTSOCKET& uid,
        const std::set<UDTSOCKET>& watch,
        std::set<UDTSOCKET>& result,
        bool enable);
};
