#include "reverse_connection_pool.h"

#include <nx/network/socket_global.h>
#include <nx/utils/std/future.h>

#include "../outgoing_tunnel_pool.h"
#include "reverse_connection_holder.h"

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

static String getHostSuffix(const String& hostName)
{
    const auto dotIndex = hostName.indexOf('.');
    if (dotIndex != -1)
        return hostName.right(hostName.length() - dotIndex - 1);

    return String();
}

ReverseConnectionPool::ReverseConnectionPool(
    aio::AIOService* aioService,
    std::unique_ptr<hpm::api::AbstractMediatorClientTcpConnection> mediatorConnection)
:
    base_type(aioService, nullptr),
    m_mediatorConnection(std::move(mediatorConnection)),
    m_acceptor(
        [this](String hostName, std::unique_ptr<AbstractStreamSocket> socket)
        {
            NX_LOGX(lm("New socket(%1) from %2").args(socket, hostName), cl_logDEBUG2);
            getOrCreateHolder(hostName)->saveSocket(std::move(socket));
        }),
    m_isReconnectHandlerSet(false),
    m_startTime(std::chrono::steady_clock::now()),
    m_startTimeout(0)
{
    bindToAioThread(getAioThread());
}

ReverseConnectionPool::~ReverseConnectionPool()
{
    pleaseStopSync(false);
}

void ReverseConnectionPool::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_mediatorConnection->bindToAioThread(aioThread);
    m_acceptor.bindToAioThread(aioThread);
}

bool ReverseConnectionPool::start(HostAddress publicIp, uint16_t port, bool waitForRegistration)
{
    m_publicIp = std::move(publicIp);
    SocketAddress serverAddress(HostAddress::anyHost, port);
    if (!m_acceptor.start(
            SocketGlobals::outgoingTunnelPool().ownPeerId(),
            serverAddress, m_mediatorConnection->getAioThread()))
    {
        NX_LOGX(lm("Could not start acceptor on %1: %2")
            .args(serverAddress, SystemError::getLastOSErrorText()), cl_logWARNING);

        return false;
    }

    return registerOnMediator(waitForRegistration);
}

uint16_t ReverseConnectionPool::port() const
{
    return m_acceptor.address().port;
}

std::shared_ptr<ReverseConnectionSource>
    ReverseConnectionPool::getConnectionSource(const String& hostName)
{
    QnMutexLocker lk(&m_mutex);
    const auto suffix = getHostSuffix(hostName);
    if (suffix.isEmpty())
    {
        // Given hostName is a suffix itself, search for any.
        const auto suffixIterator = m_connectionHolders.find(hostName);
        if (suffixIterator == m_connectionHolders.end())
        {
            NX_LOGX(lm("No holders by host suffix %1").arg(hostName), cl_logDEBUG2);
            return nullptr;
        }

        for (const auto& host: suffixIterator->second)
        {
            if (const auto connections = host.second->socketCount())
            {
                NX_LOGX(lm("Return holder for %1 by suffix %2 with %3 connections(s)")
                    .args(host.first, hostName, connections), cl_logDEBUG1);
                return host.second;
            }
        }

        NX_LOGX(lm("No connections on %1 holder(s) by suffix %2")
            .args(suffixIterator->second.size(), hostName), cl_logDEBUG1);
        return nullptr;
    }
    else
    {
        const auto suffixIterator = m_connectionHolders.find(suffix);
        if (suffixIterator == m_connectionHolders.end())
        {
            NX_LOGX(lm("No holders by suffix %1 of %2").args(suffix, hostName), cl_logDEBUG2);
            return nullptr;
        }

        const auto hostIterator = suffixIterator->second.find(hostName);
        if (hostIterator == suffixIterator->second.end())
        {
            NX_LOGX(lm("No holders for host %1").arg(hostName), cl_logDEBUG2);
            return nullptr;
        }

        // TODO: #ak Not active objects MUST be removed, not stored forever.
        if (hostIterator->second->isActive())
        {
            NX_LOGX(lm("Return holder for %1 with %2 connection(s)")
                .args(hostName, hostIterator->second->socketCount()), cl_logDEBUG1);
            return hostIterator->second;
        }

        NX_LOGX(lm("No connections on holder for %1").args(hostName), cl_logDEBUG1);
        return nullptr;
    }
}

void ReverseConnectionPool::setPoolSize(boost::optional<size_t> value)
{
    m_acceptor.setPoolSize(std::move(value));

    // TODO: also need to close extra connections in m_connectionHolders
}

void ReverseConnectionPool::setKeepAliveOptions(boost::optional<KeepAliveOptions> value)
{
    m_acceptor.setKeepAliveOptions(std::move(value));
}

void ReverseConnectionPool::setStartTimeout(std::chrono::milliseconds value)
{
    m_startTime = std::chrono::steady_clock::now();
    m_startTimeout = value;
}

void ReverseConnectionPool::stopWhileInAioThread()
{
    m_mediatorConnection.reset();
    m_acceptor.pleaseStopSync();

    // Holders are need to be stopped here, as other shared_ptrs are not primary.
    for (auto& suffix: m_connectionHolders)
    {
        for (auto& holder: suffix.second)
            holder.second->pleaseStopSync();
    }

    m_connectionHolders.clear();
}

bool ReverseConnectionPool::registerOnMediator(bool waitForRegistration)
{
    std::shared_ptr<utils::promise<bool>> registrationPromise;
    if (waitForRegistration)
        registrationPromise = std::make_shared<utils::promise<bool>>();

    hpm::api::ClientBindRequest request;
    request.originatingPeerID = m_acceptor.selfHostName();
    request.tcpReverseEndpoints.push_back(SocketAddress(m_publicIp, m_acceptor.address().port));
    m_mediatorConnection->bind(
        std::move(request),
        [this, registrationPromise](
            hpm::api::ResultCode code, hpm::api::ClientBindResponse responce)
        {
            if (code == nx::hpm::api::ResultCode::ok)
            {
                NX_LOGX(lm("Registred on mediator by %1 with %2")
                    .args(m_acceptor.selfHostName(), m_acceptor.address()), cl_logINFO);

                if (auto& options = responce.tcpConnectionKeepAlive)
                    m_mediatorConnection->client()->setKeepAliveOptions(std::move(*options));
            }
            else
            {
                NX_LOGX(lm("Could not register on mediator by %1: %2")
                    .args(m_acceptor.selfHostName(), code),
                    (std::chrono::steady_clock::now() - m_startTime > m_startTimeout)
                        ? cl_logWARNING : cl_logDEBUG1);
            }

            if (!m_isReconnectHandlerSet)
            {
                m_mediatorConnection->setOnReconnectedHandler([this](){ registerOnMediator(); });
                m_isReconnectHandlerSet = true;
            }

            if (registrationPromise)
                registrationPromise->set_value(code == nx::hpm::api::ResultCode::ok);
        });

    if (registrationPromise)
        return registrationPromise->get_future().get();

    return true;
}

std::shared_ptr<ReverseConnectionHolder>
    ReverseConnectionPool::getOrCreateHolder(const String& hostName)
{
    QnMutexLocker lk(&m_mutex);
    auto& holdersInSuffix = m_connectionHolders[getHostSuffix(hostName)];
    auto it = holdersInSuffix.find(hostName);
    if (it == holdersInSuffix.end())
    {
        it = holdersInSuffix.emplace(
            std::move(hostName),
            std::make_shared<ReverseConnectionHolder>(
                m_mediatorConnection->getAioThread(),
                hostName)).first;
    }

    return it->second;
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
