#include "epoll_impl.h"

#include "../common.h"

EpollImpl::EpollImpl():
    m_iLocalID(0)
{
}

int EpollImpl::add_usock(const UDTSOCKET& u, const int* events)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!events || (*events & UDT_EPOLL_IN))
        m_sUDTSocksIn.insert(u);
    if (!events || (*events & UDT_EPOLL_OUT))
        m_sUDTSocksOut.insert(u);

    return 0;
}

int EpollImpl::remove_usock(const UDTSOCKET& u)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sUDTSocksIn.erase(u);
    m_sUDTSocksOut.erase(u);
    m_sUDTSocksEx.erase(u);

    return 0;
}

void EpollImpl::removeSocketEvents(const UDTSOCKET& socket)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sUDTReads.erase(socket);
    m_sUDTWrites.erase(socket);
    m_sUDTExcepts.erase(socket);
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
