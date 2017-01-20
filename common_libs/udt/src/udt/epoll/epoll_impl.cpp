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

    return 0;
}

void EpollImpl::removeUdtSocketEvents(const UDTSOCKET& socket)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sUDTReads.erase(socket);
    m_sUDTWrites.erase(socket);
    m_sUDTExcepts.erase(socket);
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
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_sUDTSocksIn.empty() && m_sUDTSocksOut.empty() &&
            m_systemEpoll->socketsPolledCount() == 0 && (msTimeout < 0))
        {
            // No socket is being monitored, this may be a deadlock.
            throw CUDTException(5, 3);
        }
    }

    // Clear these sets in case the app forget to do it.
    if (udtReadFds)
        udtReadFds->clear();
    if (udtWriteFds)
        udtWriteFds->clear();
    if (systemReadFds)
        systemReadFds->clear();
    if (systemWriteFds)
        systemWriteFds->clear();

#if 1
    const std::chrono::microseconds timeout = 
        msTimeout < 0
        ? std::chrono::microseconds::max()
        : std::chrono::milliseconds(msTimeout);

    int eventCount = m_systemEpoll->poll(systemReadFds, systemWriteFds, timeout);
    if (eventCount < 0)
        return -1;

    eventCount += addUdtSocketEvents(udtReadFds, udtWriteFds);
    return eventCount;

#else

    int total = 0;

    uint64_t entertime = CTimer::getTime();
    for (;;)
    {
        total += addUdtSocketEvents(udtReadFds, udtWriteFds);
        
        if (systemReadFds || systemWriteFds)
        {
            const int eventCount = m_systemEpoll->poll(
                systemReadFds,
                systemWriteFds,
                std::chrono::microseconds::zero());
            if (eventCount < 0)
                return -1;
            total += eventCount;
        }

        if (total > 0)
            return total;

        auto now = CTimer::getTime();
        if (msTimeout >= 0 && now - entertime >= (uint64_t)msTimeout * 1000)
            return 0;

        CTimer::waitForEvent();
    }

    return 0;
#endif
}

void EpollImpl::updateEpollSets(int events, const UDTSOCKET& socketId, bool enable)
{
    bool modified = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if ((events & UDT_EPOLL_IN) != 0)
            modified |= updateEpollSets(socketId, m_sUDTSocksIn, m_sUDTReads, enable);

        if ((events & UDT_EPOLL_OUT) != 0)
            modified |= updateEpollSets(socketId, m_sUDTSocksOut, m_sUDTWrites, enable);

        if ((events & UDT_EPOLL_ERR) != 0)
            modified |= updateEpollSets(socketId, m_sUDTSocksEx, m_sUDTExcepts, enable);
    }
    
    if (modified)
    {
        m_systemEpoll->interrupt();
        // TODO Triggering events.
        CTimer::triggerEvent();
    }
}

bool EpollImpl::updateEpollSets(
    const UDTSOCKET& uid,
    const std::set<UDTSOCKET>& watch,
    std::set<UDTSOCKET>& result,
    bool enable)
{
    bool modified = false;
    if (enable && (watch.find(uid) != watch.end()))
    {
        result.insert(uid);
        modified = true;
    }
    else if (!enable)
    {
        if (result.erase(uid) > 0)
            modified = true;
    }

    return modified;
}

int EpollImpl::addUdtSocketEvents(
    std::map<UDTSOCKET, int>* udtReadFds,
    std::map<UDTSOCKET, int>* udtWriteFds)
{
    // Sockets with exceptions are returned to both read and write sets.

    int total = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if ((NULL != udtReadFds) && (!m_sUDTReads.empty() || !m_sUDTExcepts.empty()))
        {
            udtReadFds->clear();
            for (const auto& handle : m_sUDTReads)
                udtReadFds->emplace(handle, UDT_EPOLL_IN);
            for (std::set<UDTSOCKET>::const_iterator i = m_sUDTExcepts.begin(); i != m_sUDTExcepts.end(); ++i)
                (*udtReadFds)[*i] |= UDT_EPOLL_ERR;
            total += m_sUDTReads.size() + m_sUDTExcepts.size();
        }

        if ((NULL != udtWriteFds) && (!m_sUDTWrites.empty() || !m_sUDTExcepts.empty()))
        {
            for (const auto& handle : m_sUDTWrites)
                udtWriteFds->emplace(handle, UDT_EPOLL_OUT);
            for (std::set<UDTSOCKET>::const_iterator i = m_sUDTExcepts.begin(); i != m_sUDTExcepts.end(); ++i)
                (*udtWriteFds)[*i] |= UDT_EPOLL_ERR;
            total += m_sUDTWrites.size() + m_sUDTExcepts.size();
        }
    }

    // Somehow, connection failure event can be not reported, so checking additionally.
    for (auto& socketHandleAndEventMask: *udtWriteFds)
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

    return total;
}
