#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "abstract_epoll.h"
#include "../udt.h"

class EpollImpl
{
public:
    EpollImpl();
    virtual ~EpollImpl();

    int addUdtSocket(const UDTSOCKET& u, const int* events);
    int removeUdtSocket(const UDTSOCKET& u);
    void removeUdtSocketEvents(const UDTSOCKET& socket);

    void add(const SYSSOCKET& s, const int* events);
    void remove(const SYSSOCKET& s);

    int wait(
        std::map<UDTSOCKET, int>* udtReadFds, std::map<UDTSOCKET, int>* udtWriteFds,
        int64_t msTimeout,
        std::map<SYSSOCKET, int>* systemReadFds, std::map<SYSSOCKET, int>* systemWriteFds);

    void updateEpollSets(int events, const UDTSOCKET& socketId, bool enable);

private:
    std::mutex m_mutex;

    std::unique_ptr<AbstractEpoll> m_systemEpoll;

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
