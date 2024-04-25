// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "outgoing_tunnel_pool.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::cloud {

OutgoingTunnelPool::OutgoingTunnelPool(const CloudConnectSettings& settings):
    m_settings(settings),
    m_ownPeerId(nx::Uuid::createUuid().toSimpleString().toUtf8())
{
}

OutgoingTunnelPool::~OutgoingTunnelPool()
{
    m_counter.wait();

    NX_ASSERT(m_terminated);
    NX_ASSERT(m_pool.empty());
}

void OutgoingTunnelPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    // Stopping all tunnels. Assuming no one calls establishNewConnection anymore.
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_stopping = true;
    nx::utils::BarrierHandler tunnelsStoppedFuture(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            tunnelsStopped(std::move(completionHandler));
        });

    decltype(m_pool) pool;
    m_pool.swap(pool);
    for (auto& tunnelData: pool)
    {
        auto tunnelContext = std::move(tunnelData.second);
        auto tunnelPtr = tunnelContext->tunnel.get();
        tunnelPtr->pleaseStop(
            [handler = tunnelsStoppedFuture.fork(),
                tunnelContext = std::move(tunnelContext)]() mutable
            {
                tunnelContext->tunnel.reset();
                handler();
            });
    }
}

void OutgoingTunnelPool::establishNewConnection(
    const AddressEntry& targetHostAddress,
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    AbstractOutgoingTunnel::NewConnectionHandler handler)
{
    NX_ASSERT(!m_terminated && !m_stopping);

    NX_MUTEX_LOCKER lock(&m_mutex);

    auto& tunnelContext = getTunnel(targetHostAddress);
    tunnelContext.handlers.push_back(std::move(handler));

    tunnelContext.tunnel->establishNewConnection(
        std::move(timeout),
        std::move(socketAttributes),
        [this, tunnelContextPtr = &tunnelContext, handlerIter = --tunnelContext.handlers.end()](
            SystemError::ErrorCode sysErrorCode,
            TunnelAttributes tunnelAttributes,
            std::unique_ptr<AbstractStreamSocket> connection)
        {
            reportConnectionResult(
                sysErrorCode,
                std::move(tunnelAttributes),
                std::move(connection),
                tunnelContextPtr,
                handlerIter);
        });
}

std::string OutgoingTunnelPool::ownPeerId() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_isOwnPeerIdAssigned)
    {
        NX_ASSERT(false, "Own peer id is not supposed to be used until it's assigned");

        m_isOwnPeerIdAssigned = true; //< Peer id is not supposed to be changed after first use.
        NX_INFO(this, nx::format("Random own peer id: %1").arg(m_ownPeerId));
    }

    return m_ownPeerId;
}

void OutgoingTunnelPool::assignOwnPeerId(const std::string& name, const nx::Uuid& uuid)
{
    NX_ASSERT(!uuid.isNull());
    const auto id = nx::utils::buildString(
        name, '_',
        uuid.toSimpleStdString(), '_',
        std::to_string(nx::utils::random::number()));

    setOwnPeerId(id);
}

void OutgoingTunnelPool::setOwnPeerId(const std::string& peerId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_isOwnPeerIdAssigned)
    {
        if (s_isIgnoringOwnPeerIdChange)
            return;
        NX_ASSERT(false, "Own peer id is not supposed to be changed");
    }
    else
    {
        m_isOwnPeerIdAssigned = true;
        m_ownPeerId = peerId;
        NX_INFO(this, nx::format("Assigned own peer id: %1").arg(m_ownPeerId));
    }
}

void OutgoingTunnelPool::clearOwnPeerIdIfEqual(const std::string& name, const nx::Uuid& uuid)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_isOwnPeerIdAssigned &&
        nx::utils::startsWith(m_ownPeerId, nx::utils::buildString(name, '_', uuid.toSimpleStdString())))
    {
        m_isOwnPeerIdAssigned = false;
        m_ownPeerId.clear();
    }
}

void OutgoingTunnelPool::removeAllTunnelsSync()
{
    pleaseStopSync();

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_terminated = false;
    m_stopping = false;
}

OutgoingTunnelPool::OnTunnelClosedSubscription& OutgoingTunnelPool::onTunnelClosedSubscription()
{
    return m_onTunnelClosedSubscription;
}

void OutgoingTunnelPool::ignoreOwnPeerIdChange()
{
    s_isIgnoringOwnPeerIdChange = true;
}

OutgoingTunnelPool::TunnelContext&
    OutgoingTunnelPool::getTunnel(const AddressEntry& targetHostAddress)
{
    const auto iterAndInsertionResult = m_pool.emplace(
        targetHostAddress.host.toString(),
        nullptr);
    if (!iterAndInsertionResult.second)
        return *iterAndInsertionResult.first->second;

    NX_VERBOSE(this, nx::format("Creating outgoing tunnel to host %1").
            arg(targetHostAddress.host.toString()));

    auto tunnel = OutgoingTunnelFactory::instance().create(m_settings, targetHostAddress);
    tunnel->bindToAioThread(SocketGlobals::aioService().getRandomAioThread());
    tunnel->setOnClosedHandler(
        std::bind(&OutgoingTunnelPool::onTunnelClosed, this, tunnel.get()));

    iterAndInsertionResult.first->second = std::make_unique<TunnelContext>();
    iterAndInsertionResult.first->second->tunnel = std::move(tunnel);
    return *iterAndInsertionResult.first->second;
}

void OutgoingTunnelPool::reportConnectionResult(
    SystemError::ErrorCode sysErrorCode,
    TunnelAttributes tunnelAttributes,
    std::unique_ptr<AbstractStreamSocket> connection,
    TunnelContext* tunnelContext,
    std::list<AbstractOutgoingTunnel::NewConnectionHandler>::iterator handlerIter)
{
    AbstractOutgoingTunnel::NewConnectionHandler userHandler;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        userHandler.swap(*handlerIter);
        tunnelContext->handlers.erase(handlerIter);
    }

    userHandler(
        sysErrorCode,
        std::move(tunnelAttributes),
        std::move(connection));
}

void OutgoingTunnelPool::onTunnelClosed(AbstractOutgoingTunnel* tunnelPtr)
{
    const auto callLocker = m_counter.getScopedIncrement();

    std::list<AbstractOutgoingTunnel::NewConnectionHandler> userHandlers;
    std::unique_ptr<AbstractOutgoingTunnel> tunnel;
    std::string remoteHostName;

    {
        NX_MUTEX_LOCKER lk(&m_mutex);

        TunnelDictionary::iterator tunnelIter = m_pool.end();
        for (auto it = m_pool.begin(); it != m_pool.end(); ++it)
        {
            if (it->second->tunnel.get() == tunnelPtr)
            {
                tunnelIter = it;
                break;
            }
        }

        if (m_stopping)
            return; //tunnel is being cancelled?

        NX_VERBOSE(this, nx::format("Removing tunnel to host %1").arg(tunnelIter->first));
        tunnel.swap(tunnelIter->second->tunnel);
        userHandlers.swap(tunnelIter->second->handlers);
        remoteHostName = tunnelIter->first;
        m_pool.erase(tunnelIter);
    }

    tunnel.reset();

    // Reporting error to awaiting users.
    for (auto& handler: userHandlers)
        handler(SystemError::interrupted, TunnelAttributes(), nullptr);

    m_onTunnelClosedSubscription.notify(std::move(remoteHostName));
}

void OutgoingTunnelPool::tunnelsStopped(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_aioThreadBinder.post(
        [this, completionHandler = std::move(completionHandler)]
        {
            m_terminated = true;
            m_aioThreadBinder.pleaseStopSync();
            completionHandler();
        });
}

bool OutgoingTunnelPool::s_isIgnoringOwnPeerIdChange(false);

} // namespace nx::network::cloud
