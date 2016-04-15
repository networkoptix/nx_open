/**********************************************************
* Apr 13, 2016
* akolesnikov
***********************************************************/

#include "udp_hole_punching_rendezvous_connector.h"

#include <nx/utils/log/log.h>

#include <utils/common/cpp14.h>


namespace nx {
namespace network {
namespace cloud {

UdpHolePunchingRendezvousConnector::UdpHolePunchingRendezvousConnector(
    nx::String connectSessionId,
    SocketAddress remotePeerAddress,
    std::unique_ptr<nx::network::UDPSocket> udpSocket)
:
    m_connectSessionId(std::move(connectSessionId)),
    m_remotePeerAddress(std::move(remotePeerAddress)),
    m_udpSocket(std::move(udpSocket))
{
}

UdpHolePunchingRendezvousConnector::~UdpHolePunchingRendezvousConnector()
{
    m_udtConnection.reset();
}

void UdpHolePunchingRendezvousConnector::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_aioThreadBinder.pleaseStop(
        [this, completionHandler = std::move(completionHandler)]()
        {
            m_udtConnection.reset();
        });
}

aio::AbstractAioThread* UdpHolePunchingRendezvousConnector::getAioThread()
{
    return m_aioThreadBinder.getAioThread();
}

void UdpHolePunchingRendezvousConnector::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    m_aioThreadBinder.bindToAioThread(aioThread);
}

void UdpHolePunchingRendezvousConnector::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioThreadBinder.post(std::move(func));
}

void UdpHolePunchingRendezvousConnector::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioThreadBinder.dispatch(std::move(func));
}

void UdpHolePunchingRendezvousConnector::connect(
    std::chrono::milliseconds timeout,
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::unique_ptr<UdtStreamSocket>)> completionHandler)
{
    post(   //just to simplify code (get rid of synchronization)
        [this, timeout, completionHandler = std::move(completionHandler)]() mutable
        {
            auto udtConnection = std::make_unique<UdtStreamSocket>();
            udtConnection->bindToAioThread(m_aioThreadBinder.getAioThread());
            bool result = true;
            //moving system socket handler from m_mediatorUdpClient to m_udtConnection
            if (m_udpSocket)
                result = udtConnection->bindToUdpSocket(std::move(*m_udpSocket));
            m_udpSocket.reset();
            if (!result ||
                !udtConnection->setRendezvous(true) ||
                !udtConnection->setNonBlockingMode(true) ||
                !udtConnection->setSendTimeout(timeout.count()))
            {
                const auto errorCode = SystemError::getLastOSErrorCode();
                NX_LOGX(lm("session %1. Failed to create UDT socket. %2")
                    .arg(m_connectSessionId).arg(SystemError::toString(errorCode)),
                    cl_logDEBUG1);
                return completionHandler(
                    errorCode,
                    nullptr);
            }

            m_completionHandler = std::move(completionHandler);
            m_udtConnection = std::move(udtConnection);
            m_udtConnection->connectAsync(
                m_remotePeerAddress,
                [this](SystemError::ErrorCode errorCode)
                {
                    //need post to be sure that aio subsystem does not use socket anymore
                    post(
                        std::bind(
                            &UdpHolePunchingRendezvousConnector::onUdtConnectFinished,
                            this,
                            errorCode));
                });
        });
}

SocketAddress UdpHolePunchingRendezvousConnector::remoteAddress() const
{
    return m_remotePeerAddress;
}

void UdpHolePunchingRendezvousConnector::onUdtConnectFinished(
    SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("session %1. Error establishing rendezvous udt connection to %2: %3")
            .arg(m_connectSessionId).arg(m_remotePeerAddress.toString())
            .arg(SystemError::toString(errorCode)),
            cl_logDEBUG2);
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(errorCode, nullptr);
        return;
    }

    //TODO #ak validating that remote side uses same connection id

    NX_LOGX(lm("session %1. Successfully established rendezvous udt connection to %2")
        .arg(m_connectSessionId).arg(m_remotePeerAddress.toString()),
        cl_logDEBUG2);

    auto udtConnection = std::move(m_udtConnection);
    auto completionHandler = std::move(m_completionHandler);
    completionHandler(SystemError::noError, std::move(udtConnection));
}

} // namespace cloud
} // namespace network
} // namespace nx
