#include "outgoing_tunnel_pool.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {

OutgoingTunnelPool::OutgoingTunnelPool():
    m_isOwnPeerIdAssigned(false),
    m_ownPeerId(QnUuid::createUuid().toSimpleString().toUtf8()),
    m_terminated(false),
    m_stopping(false)
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
    //stopping all tunnels. Assuming no one calls establishNewConnection anymore
    QnMutexLocker lock(&m_mutex);
    m_stopping = true;
    nx::utils::BarrierHandler tunnelsStoppedFuture(
        [this, completionHandler = move(completionHandler)]() mutable
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
    using namespace std::placeholders;

    NX_ASSERT(!m_terminated && !m_stopping);

    QnMutexLocker lock(&m_mutex);

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

String OutgoingTunnelPool::ownPeerId() const
{
    QnMutexLocker lock(&m_mutex);
    if (!m_isOwnPeerIdAssigned)
    {
        NX_ASSERT(false, "Own peer id is not supposed to be used until it's assigned");

        m_isOwnPeerIdAssigned = true; //< Peer id is not supposed to be changed after first use.
        NX_LOGX(lm("Random own peer id: %1").arg(m_ownPeerId), cl_logINFO);
    }

    return m_ownPeerId;
}

void OutgoingTunnelPool::assignOwnPeerId(const String& name, const QnUuid& uuid)
{
    const auto id = lm("%1_%2_%3")
        .args(name, uuid.toSimpleString(), nx::utils::random::number()).toUtf8();
    setOwnPeerId(id);
}

void OutgoingTunnelPool::setOwnPeerId(const String& peerId)
{
    QnMutexLocker lock(&m_mutex);

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
        NX_LOGX(lm("Assigned own peer id: %1").arg(m_ownPeerId), cl_logINFO);
    }
}

void OutgoingTunnelPool::clearOwnPeerIdIfEqual(const String& name, const QnUuid& uuid)
{
    QnMutexLocker lock(&m_mutex);

    if (m_isOwnPeerIdAssigned &&
        m_ownPeerId.startsWith(lm("%1_%2").args(name, uuid.toSimpleString()).toUtf8()))
    {
        m_isOwnPeerIdAssigned = false;
        m_ownPeerId.clear();
    }
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

    NX_LOGX(
        lm("Creating outgoing tunnel to host %1").
            arg(targetHostAddress.host.toString()),
        cl_logDEBUG1);

    auto tunnel = OutgoingTunnelFactory::instance().create(targetHostAddress);
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
        QnMutexLocker lock(&m_mutex);
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
    QString remoteHostName;

    {
        QnMutexLocker lk(&m_mutex);

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

        NX_LOGX(lm("Removing tunnel to host %1").arg(tunnelIter->first), cl_logDEBUG1);
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

} // namespace cloud
} // namespace network
} // namespace nx
