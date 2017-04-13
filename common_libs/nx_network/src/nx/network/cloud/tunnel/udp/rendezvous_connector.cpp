#include "rendezvous_connector.h"

#include <nx/utils/log/log.h>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

RendezvousConnector::RendezvousConnector(
    nx::String connectSessionId,
    SocketAddress remotePeerAddress,
    std::unique_ptr<nx::network::UDPSocket> udpSocket)
:
    m_connectSessionId(std::move(connectSessionId)),
    m_remotePeerAddress(std::move(remotePeerAddress)),
    m_udpSocket(std::move(udpSocket))
{
}

RendezvousConnector::RendezvousConnector(
    nx::String connectSessionId,
    SocketAddress remotePeerAddress,
    SocketAddress localAddressToBindTo)
:
    m_connectSessionId(std::move(connectSessionId)),
    m_remotePeerAddress(std::move(remotePeerAddress)),
    m_localAddressToBindTo(std::move(localAddressToBindTo))
{
}

RendezvousConnector::~RendezvousConnector()
{
    m_udtConnection.reset();
}

void RendezvousConnector::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_aioThreadBinder.pleaseStop(
        [this, completionHandler = std::move(completionHandler)]()
        {
            m_udtConnection.reset();
        });
}

aio::AbstractAioThread* RendezvousConnector::getAioThread() const
{
    return m_aioThreadBinder.getAioThread();
}

void RendezvousConnector::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    m_aioThreadBinder.bindToAioThread(aioThread);
}

void RendezvousConnector::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioThreadBinder.post(std::move(func));
}

void RendezvousConnector::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioThreadBinder.dispatch(std::move(func));
}

void RendezvousConnector::connect(
    std::chrono::milliseconds timeout,
    ConnectCompletionHandler completionHandler)
{
    post(   //just to simplify code (get rid of synchronization)
        [this, timeout, completionHandler = std::move(completionHandler)]() mutable
        {
            auto udtConnection = std::make_unique<UdtStreamSocket>(SocketFactory::udpIpVersion());
            udtConnection->bindToAioThread(m_aioThreadBinder.getAioThread());
            if (!initializeUdtConnection(udtConnection.get(), timeout))
            {
                const auto sysErrorCode = SystemError::getLastOSErrorCode();
                NX_LOGX(lm("session %1. Failed to create UDT socket. %2")
                    .arg(m_connectSessionId).arg(SystemError::toString(sysErrorCode)),
                    cl_logDEBUG1);
                return completionHandler(sysErrorCode);
            }

            m_completionHandler = std::move(completionHandler);
            m_udtConnection = std::move(udtConnection);
            m_udtConnection->connectAsync(
                m_remotePeerAddress,
                [this](SystemError::ErrorCode errorCode)
                {
                    //need post to be sure that aio subsystem does not use socket anymore
                    post(std::bind(&RendezvousConnector::onUdtConnectFinished, this, errorCode));
                });
        });
}

std::unique_ptr<nx::network::UdtStreamSocket> RendezvousConnector::takeConnection()
{
    return std::move(m_udtConnection);
}

const nx::String& RendezvousConnector::connectSessionId() const
{
    return m_connectSessionId;
}

const SocketAddress& RendezvousConnector::remoteAddress() const
{
    return m_remotePeerAddress;
}

bool RendezvousConnector::initializeUdtConnection(
    UdtStreamSocket* udtConnection,
    std::chrono::milliseconds timeout)
{
    auto udpSocket = std::move(m_udpSocket);
    if (udpSocket)
    {
        // Moving system socket handler from m_mediatorUdpClient to m_udtConnection.
        const bool result = udtConnection->bindToUdpSocket(std::move(*udpSocket));
        // udpSocket destructor may reset system error code.
        const auto sysErrorCodeBak = SystemError::getLastOSErrorCode();
        udpSocket.reset();
        SystemError::setLastErrorCode(sysErrorCodeBak);
        if (!result)
            return false;
    }

    if (m_localAddressToBindTo)
    {
        if (!udtConnection->bind(*m_localAddressToBindTo))
            return false;
    }

    if (!udtConnection->setRendezvous(true) ||
        !udtConnection->setNonBlockingMode(true) ||
        !udtConnection->setSendTimeout(timeout.count()))
    {
        return false;
    }

    return true;
}

void RendezvousConnector::onUdtConnectFinished(
    SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. Error establishing rendezvous udt connection to %2: %3")
            .arg(m_connectSessionId).arg(m_remotePeerAddress.toString())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG2);
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(errorCode);
        return;
    }

    //TODO #ak Validating that remote side uses same connection id.

    NX_LOGX(lm("session %1. Successfully established rendezvous udt connection to %2")
        .arg(m_connectSessionId).arg(m_remotePeerAddress.toString()),
        cl_logDEBUG2);

    auto completionHandler = std::move(m_completionHandler);
    completionHandler(SystemError::noError);
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
