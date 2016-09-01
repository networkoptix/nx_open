#include "reverse_connection_pool.h"
#include "reverse_connection_holder.h"

#include <nx/network/cloud/data/client_bind_data.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseConnectionPool::ReverseConnectionPool(
    std::shared_ptr<MediatorConnection> mediatorConnection)
:
    m_mediatorConnection(std::move(mediatorConnection)),
    m_acceptor(
        QnUuid::createUuid().toByteArray(),
        [this](String hostName, std::unique_ptr<AbstractStreamSocket> socket)
        {
          NX_LOGX(lm("New socket(%1) from '%2").args(socket, hostName), cl_logDEBUG1);
          getHolder(hostName, true)->saveSocket(std::move(socket));
        })
{
}

bool ReverseConnectionPool::start(HostAddress publicIp, uint16_t port, bool waitForRegistration)
{
    m_publicIp = std::move(publicIp);
    SocketAddress serverAddress(HostAddress::anyHost, port);
    if (!m_acceptor.start(serverAddress, m_mediatorConnection->getAioThread()))
    {
        NX_LOGX(lm("Could not start acceptor: %1").arg(SystemError::getLastOSErrorText()),
            cl_logWARNING);

        return false;
    }

    return registerOnMediator(waitForRegistration);
}

uint16_t ReverseConnectionPool::port() const
{
    return m_acceptor.address().port;
}

std::shared_ptr<ReverseConnectionHolder>
    ReverseConnectionPool::getConnectionHolder(const String& hostName)
{
    auto holder = getHolder(hostName, false);
    if (!holder)
        return nullptr;

    const auto connections = holder->socketCount();
    if (connections == 0)
        return nullptr;

    NX_LOGX(lm("Returning holder for '%1' with %2 connections(s)")
        .strs(hostName, connections), cl_logDEBUG1);

    return holder;
}

void ReverseConnectionPool::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_mediatorConnection->pleaseStop(
        [this, handler = std::move(completionHandler)]()
        {
            m_acceptor.pleaseStop();
            for (auto& holder: m_connectionHolders)
                holder.second->stopInAioThread();

            m_connectionHolders.clear();
            handler();
        });
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

bool ReverseConnectionPool::registerOnMediator(bool waitForRegistration)
{
    std::shared_ptr<utils::promise<bool>> registrationPromise;
    if (waitForRegistration)
        registrationPromise = std::make_shared<utils::promise<bool>>();

    hpm::api::ClientBindRequest request;
    request.originatingPeerID = m_acceptor.selfHostName();
    request.tcpReverseEndpoints.push_back(SocketAddress(m_publicIp, m_acceptor.address().port));
    m_mediatorConnection->send(
        std::move(request),
        [this, registrationPromise](nx::hpm::api::ResultCode code)
        {
            if (code == nx::hpm::api::ResultCode::ok)
            {
                NX_LOGX(lm("Registred on mediator by %1 with %2")
                    .strs(m_acceptor.selfHostName(), m_acceptor.address()), cl_logINFO);
            }
            else
            {
                NX_LOGX(lm("Could not register on mediator: %1").str(code), cl_logWARNING);
            }

            // Reregister on reconnect:
            m_mediatorConnection->setOnReconnectedHandler([this](){ registerOnMediator(); });

            if (registrationPromise)
                registrationPromise->set_value(code == nx::hpm::api::ResultCode::ok);
        });

    if (registrationPromise)
        return registrationPromise->get_future().get();

    return true;
}

std::shared_ptr<ReverseConnectionHolder>
    ReverseConnectionPool::getHolder(const String& hostName, bool mayCreate)
{
    QnMutexLocker lk(&m_mutex);
    auto it = m_connectionHolders.find(hostName);
    if (it == m_connectionHolders.end())
    {
        if (!mayCreate)
            return nullptr;

        it = m_connectionHolders.emplace(
            std::move(hostName),
            std::make_shared<ReverseConnectionHolder>(
                m_mediatorConnection->getAioThread())).first;

        // Register the same holder for system if it was not registred yet:
        const auto split = hostName.split('.');
        if (split.size() == 2)
        {
            auto& systemId = split.last();
            if (m_connectionHolders.find(systemId) == m_connectionHolders.end())
                m_connectionHolders.emplace(std::move(systemId), it->second);
        }
    }

    return it->second;
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
