#include "epoll_impl.h"

#include "../common.h"
#include "epoll_factory.h"

EpollImpl::EpollImpl()
{
    m_systemEpoll = EpollFactory::instance()->create();
}

EpollImpl::~EpollImpl()
{
}

int EpollImpl::addUdtSocket(const UDTSOCKET& u, const int* events)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!events || (*events & UDT_EPOLL_IN))
        m_sUDTSocksIn.insert(u);
    if (!events || (*events & UDT_EPOLL_OUT))
        m_sUDTSocksOut.insert(u);

    return 0;
}

int EpollImpl::removeUdtSocket(const UDTSOCKET& u)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sUDTSocksIn.erase(u);
    m_sUDTSocksOut.erase(u);
    m_sUDTSocksEx.erase(u);

    removeUdtSocketEvents(lock, u);

    return 0;
}

void EpollImpl::removeUdtSocketEvents(const UDTSOCKET& socket)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    removeUdtSocketEvents(lock, socket);
}

void EpollImpl::add(const SYSSOCKET& s, const int* events)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_systemEpoll->add(s, events);
}

void EpollImpl::remove(const SYSSOCKET& s)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_systemEpoll->remove(s);
}

int EpollImpl::wait(
    std::map<UDTSOCKET, int>* udtReadFds, std::map<UDTSOCKET, int>* udtWriteFds,
    int64_t msTimeout,
    std::map<SYSSOCKET, int>* systemReadFds, std::map<SYSSOCKET, int>* systemWriteFds)
{
    // Clear these sets in case the app forget to do it.
    if (udtReadFds)
        udtReadFds->clear();
    if (udtWriteFds)
        udtWriteFds->clear();
    if (systemReadFds)
        systemReadFds->clear();
    if (systemWriteFds)
        systemWriteFds->clear();

    std::chrono::microseconds timeout = 
        msTimeout < 0
        ? std::chrono::microseconds::max()
        : std::chrono::milliseconds(msTimeout);

    if (areThereSignalledUdtSockets())
    {
        // Going through system poll call anyway to check state of system sockets.
        timeout = std::chrono::microseconds::zero();
    }

    int eventCount = m_systemEpoll->poll(systemReadFds, systemWriteFds, timeout);
    if (eventCount < 0)
        return -1;

    eventCount += addUdtSocketEvents(udtReadFds, udtWriteFds);
    return eventCount;
}

int EpollImpl::interruptWait()
{
    m_systemEpoll->interrupt();
    return 0;
}

void EpollImpl::updateEpollSets(int events, const UDTSOCKET& socketId, bool enable)
{
    bool modified = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if ((events & UDT_EPOLL_IN) != 0)
            modified |= recordSocketEvent(socketId, m_sUDTSocksIn, m_sUDTReads, enable);

        if ((events & UDT_EPOLL_OUT) != 0)
            modified |= recordSocketEvent(socketId, m_sUDTSocksOut, m_sUDTWrites, enable);

        if ((events & UDT_EPOLL_ERR) != 0)
            modified |= recordSocketEvent(socketId, m_sUDTSocksEx, m_sUDTExcepts, enable);
    }
    
    if (modified)
        m_systemEpoll->interrupt();
}

bool EpollImpl::recordSocketEvent(
    const UDTSOCKET& uid,
    const std::set<UDTSOCKET>& polledSockets,
    std::set<UDTSOCKET>& triggeredSockets,
    bool enable)
{
    bool modified = false;
    if (enable && (polledSockets.find(uid) != polledSockets.end()))
    {
        triggeredSockets.insert(uid);
        modified = true;
    }
    else if (!enable)
    {
        if (triggeredSockets.erase(uid) > 0)
            modified = true;
    }

    return modified;
}

int EpollImpl::addUdtSocketEvents(
    std::map<UDTSOCKET, int>* udtReadFds,
    std::map<UDTSOCKET, int>* udtWriteFds)
{
    // Sockets with exceptions are returned to both read and write sets.

    const int total = addReportedEvents(udtReadFds, udtWriteFds);
    addMissingEvents(udtWriteFds);

    return total;
}

int EpollImpl::addReportedEvents(
    std::map<UDTSOCKET, int>* udtReadFds,
    std::map<UDTSOCKET, int>* udtWriteFds)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int total = 0;
    if ((NULL != udtReadFds) && (!m_sUDTReads.empty() || !m_sUDTExcepts.empty()))
    {
        udtReadFds->clear();
        for (const auto& handle: m_sUDTReads)
            udtReadFds->emplace(handle, UDT_EPOLL_IN);
        for (auto i = m_sUDTExcepts.begin(); i != m_sUDTExcepts.end(); ++i)
            (*udtReadFds)[*i] |= UDT_EPOLL_ERR;
        total += m_sUDTReads.size() + m_sUDTExcepts.size();
    }

    if ((NULL != udtWriteFds) && (!m_sUDTWrites.empty() || !m_sUDTExcepts.empty()))
    {
        for (const auto& handle: m_sUDTWrites)
            udtWriteFds->emplace(handle, UDT_EPOLL_OUT);
        for (auto i = m_sUDTExcepts.begin(); i != m_sUDTExcepts.end(); ++i)
            (*udtWriteFds)[*i] |= UDT_EPOLL_ERR;
        total += m_sUDTWrites.size() + m_sUDTExcepts.size();
    }

    return total;
}

void EpollImpl::addMissingEvents(std::map<UDTSOCKET, int>* udtWriteFds)
{
    // Somehow, connection failure event can be not reported, so checking additionally.
    for (auto& socketHandleAndEventMask : *udtWriteFds)
    {
        if ((socketHandleAndEventMask.second & UDT_EPOLL_ERR) > 0)
            continue;

        int socketEventMask = 0;
        int socketEventMaskSize = sizeof(socketEventMask);
        const int result = UDT::getsockopt(
            socketHandleAndEventMask.first,
            0,
            UDT_EVENT,
            &socketEventMask,
            &socketEventMaskSize);
        if (result == 0)
        {
            if ((socketEventMask & UDT_EPOLL_ERR) > 0)
                socketHandleAndEventMask.second |= UDT_EPOLL_ERR;
        }
    }
}

void EpollImpl::removeUdtSocketEvents(
    const std::lock_guard<std::mutex>& /*lock*/,
    const UDTSOCKET& socket)
{
    m_sUDTReads.erase(socket);
    m_sUDTWrites.erase(socket);
    m_sUDTExcepts.erase(socket);
}

bool EpollImpl::areThereSignalledUdtSockets() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_sUDTWrites.empty() || !m_sUDTReads.empty() || !m_sUDTExcepts.empty();
}
