// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
        // TODO: #akolesnikov This is work around gcc 4.8 bug. Return to lambda in gcc 4.9.
        typedef std::unique_ptr<SocketServerType>(*RealFactoryFuncType)(Args...);
        m_socketServerFactory =
            std::bind(static_cast<RealFactoryFuncType>(&realFactoryFunc), args...);
    }

    // TODO: #akolesnikov Inherit this class from QnStoppableAsync.
    void pleaseStopSync()
    {
        for (auto& other: m_appendedOthers)
            other->pleaseStopSync();
        for (auto& listener: m_listeners)
            listener->pleaseStopSync();
    }

    /**
     * @return true, if bind to every input endpoint succeeded.
     */
    template<template<typename, typename> class Dictionary, typename AllocatorType>
    bool bind(const Dictionary<SocketAddress, AllocatorType>& addrToListenList)
    {
        m_endpoints.reserve(addrToListenList.size());

        // Binding to address(es) to listen.
        for (const SocketAddress& addr: addrToListenList)
        {
            std::unique_ptr<SocketServerType> socketServer = m_socketServerFactory();
            if (!socketServer->bind(addr))
            {
                const auto osErrorCode = SystemError::getLastOSErrorCode();
                NX_ERROR(this, nx::format("Failed to bind to address %1. %2")
                    .arg(addr.toString()).arg(SystemError::toString(osErrorCode)));
                m_listeners.clear();
                return false;
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
                NX_ERROR(this, "Failed to listen address %1. %2",
                    (*it)->address().toString(), errorText);
                return false;
            }
            else
            {
                ++it;
            }
        }

        for (auto& other: m_appendedOthers)
            if (!other->listen(backlogSize))
                return false;

        return true;
    }

    template<typename Function>
    void forEachListener(const Function& function)
    {
        for (auto& listener: m_listeners)
            function(listener.get());
        for (auto& other: m_appendedOthers)
            other->forEachListener(function);
    }

    template<typename Function>
    void forEachListener(const Function& function) const
    {
        for (const auto& listener: m_listeners)
            function(listener.get());
        for (const auto& other: m_appendedOthers)
            other->forEachListener(function);
    }

    template<typename SocketServerRelatedType, typename... FuncArgs, typename... RealArgs>
    void forEachListener(
        void(SocketServerRelatedType::*function)(FuncArgs...),
        RealArgs&&... args)
    {
        for (auto& listener: m_listeners)
            (listener.get()->*function)(std::forward<RealArgs>(args)...);
        for (auto& other: m_appendedOthers)
            other->forEachListener(function, std::forward<RealArgs>(args)...);
    }

    void append(std::unique_ptr<MultiAddressServer> other)
    {
        m_endpoints.insert(
            m_endpoints.end(),
            other->m_endpoints.cbegin(),
            other->m_endpoints.cend());
        m_appendedOthers.push_back(std::move(other));
    }

    virtual Statistics statistics() const override
    {
        Statistics cumulativeStatistics;
        int totalListenersCount = addServerStatistics(*this, &cumulativeStatistics);
        for (const auto& other: m_appendedOthers)
            totalListenersCount += addServerStatistics(*other, &cumulativeStatistics);

        if (totalListenersCount > 0)
            cumulativeStatistics.requestsAveragePerConnection /= totalListenersCount;

        return cumulativeStatistics;
    }

protected:
    std::list<std::unique_ptr<SocketServerType>>& listeners()
    {
        return m_listeners;
    }

private:
    std::function<std::unique_ptr<SocketServerType>()> m_socketServerFactory;
    std::list<std::unique_ptr<SocketServerType> > m_listeners;
    std::vector<SocketAddress> m_endpoints;
    std::vector<std::unique_ptr<MultiAddressServer>> m_appendedOthers;

    static int addServerStatistics(
        const MultiAddressServer& server,
        Statistics* cumulativeStatistics)
    {
        for (const auto& listener: server.m_listeners)
        {
            const auto statistics = listener->statistics();
            cumulativeStatistics->connectionCount += statistics.connectionCount;
            cumulativeStatistics->connectionsAcceptedPerMinute +=
                statistics.connectionsAcceptedPerMinute;
            cumulativeStatistics->requestsReceivedPerMinute +=
                statistics.requestsReceivedPerMinute;
            cumulativeStatistics->requestsAveragePerConnection +=
                statistics.requestsAveragePerConnection;
        }
        return static_cast<int>(server.m_listeners.size());
    }
};

/**
 * Combines two existing servers together.
 */
template<typename SocketServerType>
std::unique_ptr<SocketServerType> catMultiAddressServers(
    std::unique_ptr<SocketServerType> one,
    std::unique_ptr<SocketServerType> two)
{
    one->append(std::move(two));
    return one;
}

} // namespace server
} // namespace network
} // namespace nx
