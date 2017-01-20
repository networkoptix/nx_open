#include "epoll_impl.h"

#include "../common.h"

EpollImpl::EpollImpl():
    m_iLocalID(0)
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

int EpollImpl::wait(
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds,
    int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    // Clear these sets in case the app forget to do it.
    if (readfds)
        readfds->clear();
    if (writefds)
        writefds->clear();
    if (lrfds)
        lrfds->clear();
    if (lwfds)
        lwfds->clear();

    int total = 0;

    uint64_t entertime = CTimer::getTime();
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_sUDTSocksIn.empty() && m_sUDTSocksOut.empty() && m_sLocals.empty() && (msTimeOut < 0))
            {
                // no socket is being monitored, this may be a deadlock
                throw CUDTException(5, 3);
            }

            // Sockets with exceptions are returned to both read and write sets.
            if ((NULL != readfds) && (!m_sUDTReads.empty() || !m_sUDTExcepts.empty()))
            {
                readfds->clear();
                for (const auto& handle : m_sUDTReads)
                    readfds->emplace(handle, UDT_EPOLL_IN);
                for (std::set<UDTSOCKET>::const_iterator i = m_sUDTExcepts.begin(); i != m_sUDTExcepts.end(); ++i)
                    (*readfds)[*i] |= UDT_EPOLL_ERR;
                total += m_sUDTReads.size() + m_sUDTExcepts.size();
            }
            if ((NULL != writefds) && (!m_sUDTWrites.empty() || !m_sUDTExcepts.empty()))
            {
                for (const auto& handle : m_sUDTWrites)
                    writefds->emplace(handle, UDT_EPOLL_OUT);
                for (std::set<UDTSOCKET>::const_iterator i = m_sUDTExcepts.begin(); i != m_sUDTExcepts.end(); ++i)
                    (*writefds)[*i] |= UDT_EPOLL_ERR;
                total += m_sUDTWrites.size() + m_sUDTExcepts.size();
            }
        }

        for (auto& socketHandleAndEventMask : *writefds)
        {
            if ((socketHandleAndEventMask.second & UDT_EPOLL_ERR) > 0)
                continue;

            //somehow, connection failure event can be not reported
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

        if (lrfds || lwfds)
        {
            const int eventCount = doSystemPoll(lrfds, lwfds);
            if (eventCount < 0)
                return -1;
            total += eventCount;
        }

        if (total > 0)
            return total;

        auto now = CTimer::getTime();
        if (msTimeOut >= 0 && now - entertime >= (uint64_t)msTimeOut * 1000)
            return 0;

        CTimer::waitForEvent();
    }

    return 0;
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
