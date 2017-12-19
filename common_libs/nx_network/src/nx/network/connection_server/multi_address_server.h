#pragma once

#include <list>
#include <memory>
#include <tuple>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_factory.h>

#include "server_statistics.h"

namespace nx {
namespace network {
namespace server {

/**
 * Listens multiple addresses by creating multiple servers (SocketServerType).
 */
template<class SocketServerType>
class MultiAddressServer:
    public AbstractStatisticsProvider
{
    template<typename... Args>
    static std::unique_ptr<SocketServerType> realFactoryFunc(Args... params)
    {
        return std::make_unique<SocketServerType>(params...);
    }

public:
    /**
     * Arguments are copied to the actual server object.
     */
    template<typename... Args>
    MultiAddressServer(Args... args)
    {
        // TODO: #ak This is work around gcc 4.8 bug. Return to lambda in gcc 4.9.
        typedef std::unique_ptr<SocketServerType>(*RealFactoryFuncType)(Args...);
        m_socketServerFactory =
            std::bind(static_cast<RealFactoryFuncType>(&realFactoryFunc), args...);
    }

    // TODO: #ak Inherit this class from QnStoppableAsync.
    void pleaseStopSync(bool assertIfCalledUnderMutex = true)
    {
        for (auto& listener: m_listeners)
            listener->pleaseStopSync(assertIfCalledUnderMutex);
    }

    /**
     * @param bindToEveryAddress If true, this method returns success if bind to every input address succeeded.
     * If false - just one successful bind is required for this method to succeed.
     */
    template<template<typename, typename> class Dictionary, typename AllocatorType>
    bool bind(
        const Dictionary<SocketAddress, AllocatorType>& addrToListenList,
        bool bindToEveryAddress = true)
    {
        m_endpoints.reserve(addrToListenList.size());

        // Binding to address(es) to listen.
        for (const SocketAddress& addr: addrToListenList)
        {
            std::unique_ptr<SocketServerType> socketServer = m_socketServerFactory();
            if (!socketServer->bind(addr))
            {
                const auto osErrorCode = SystemError::getLastOSErrorCode();
                NX_LOG(lm("Failed to bind to address %1. %2")
                    .arg(addr.toString()).arg(SystemError::toString(osErrorCode)),
                    cl_logERROR);
                if (bindToEveryAddress)
                {
                    m_listeners.clear();
                    return false;
                }
                else
                {
                    continue;
                }
            }
            m_endpoints.push_back(socketServer->address());
            m_listeners.push_back(std::move(socketServer));
        }

        return !m_listeners.empty();
    }

    const std::vector<SocketAddress>& endpoints() const
    {
        return m_endpoints;
    }

    /**
     * @return true if all bound addresses are successfully listened.
     */
    bool listen(int backlogSize = AbstractStreamServerSocket::kDefaultBacklogSize)
    {
        for (auto it = m_listeners.cbegin(); it != m_listeners.cend(); )
        {
            if (!(*it)->listen(backlogSize))
            {
                const auto& errorText = SystemError::getLastOSErrorText();
                NX_LOG(QString::fromLatin1("Failed to listen address %1. %2")
                    .arg((*it)->address().toString()).arg(errorText), cl_logERROR);
                return false;
            }
            else
            {
                ++it;
            }
        }

        return true;
    }

    template<typename Function>
    void forEachListener(const Function& function)
    {
        for (auto& listener: m_listeners)
            function(listener.get());
    }

    template<typename ... Args>
    void forEachListener(
        void(SocketServerType::*function)(Args...),
        const Args& ... args)
    {
        for (auto& listener: m_listeners)
            (listener.get()->*function)(args...);
    }

    virtual Statistics statistics() const override
    {
        Statistics cumulativeStatistics;
        for (const auto& listener: m_listeners)
        {
            const auto statistics = listener->statistics();
            cumulativeStatistics.connectionCount += statistics.connectionCount;
            cumulativeStatistics.connectionsAcceptedPerMinute +=
                statistics.connectionsAcceptedPerMinute;
            cumulativeStatistics.requestsServedPerMinute +=
                statistics.requestsServedPerMinute;
            cumulativeStatistics.requestsAveragePerConnection +=
                statistics.requestsAveragePerConnection;
        }

        if (!m_listeners.empty())
            cumulativeStatistics.requestsAveragePerConnection /= (int) m_listeners.size();

        return cumulativeStatistics;
    }

private:
    std::function<std::unique_ptr<SocketServerType>()> m_socketServerFactory;
    std::list<std::unique_ptr<SocketServerType> > m_listeners;
    std::vector<SocketAddress> m_endpoints;
};

} // namespace server
} // namespace network
} // namespace nx
