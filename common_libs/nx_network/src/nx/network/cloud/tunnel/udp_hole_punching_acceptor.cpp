#include "udp_hole_punching_acceptor.h"

#include <nx/network/cloud/cloud_config.h>
#include <nx/network/cloud/tunnel/incoming_tunnel_udt_connection.h>

#include <utils/serialization/lexical.h>

namespace nx {
namespace network {
namespace cloud {

UdpHolePunchingTunnelAcceptor::UdpHolePunchingTunnelAcceptor(
    std::list<SocketAddress> addresses)
:
    m_addresses(std::move(addresses))
{
    Q_ASSERT(m_addresses.empty());
}

void UdpHolePunchingTunnelAcceptor::accept(std::function<void(
    SystemError::ErrorCode,
    std::unique_ptr<AbstractIncomingTunnelConnection>)> handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        Q_ASSERT(!m_acceptHandler);
        Q_ASSERT(!m_udpMediatorConnection);

        m_acceptHandler = std::move(handler);
        m_udpMediatorConnection = std::make_unique<
            hpm::api::MediatorServerUdpConnection>(
                m_mediatorConnection->remoteAddress(),
                m_mediatorConnection->credentialsProvider());
    }

    hpm::api::ConnectionAckRequest ackRequest;
    ackRequest.connectSessionId = m_connectionId;
    ackRequest.connectionMethods = hpm::api::ConnectionMethod::udpHolePunching;

    m_udpMediatorConnection->connectionAck(
        std::move(ackRequest),
        [this](nx::hpm::api::ResultCode code)
        {
            if (code != hpm::api::ResultCode::ok)
            {
                NX_LOGX(lm("connectionAck error: %1")
                    .arg(QnLexical::serialized(code)), cl_logWARNING);

                // TODO: #mux code translation
                return executeAcceptHandler(SystemError::connectionAbort);
            }

            {
                QnMutexLocker lock(&m_mutex);
                if (m_stopHandler)
                    return;

                auto socket = m_udpMediatorConnection->takeSocket();
                m_udpMediatorConnection.reset();

                m_udtConnectionSocket.reset(new UdtStreamSocket);
                m_udtConnectionSocket->bindToUdpSocket(std::move(&socket));
                m_udtConnectionSocket->setNonBlockingMode(true);
            }

            initiateConnection();
        });
}

void UdpHolePunchingTunnelAcceptor::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    BarrierHandler barrier(
        [this]()
        {
            const auto handler = std::move(m_stopHandler);
            handler();
        });

    QnMutexLocker lock(&m_mutex);
    m_stopHandler = std::move(handler);

    if (m_udpMediatorConnection)
        m_udpMediatorConnection->pleaseStop(barrier.fork());

    if (m_udtConnectionSocket)
        m_udtConnectionSocket->pleaseStop(barrier.fork());
}

void UdpHolePunchingTunnelAcceptor::executeAcceptHandler(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    const auto handler = std::move(m_acceptHandler);
    return handler(errorCode, std::move(connection));
}

void UdpHolePunchingTunnelAcceptor::initiateConnection()
{
    assert(m_udtConnectionSocket->setRendezvous(true));
    assert(m_udtConnectionSocket->setSendTimeout(kHpUdtConnectTimeout.count()));
    m_udtConnectionSocket->connectAsync(
        m_addresses.front(), // TODO: #mux Can there be more then one?
        [this](SystemError::ErrorCode code)
        {
            if (code != SystemError::noError)
                return executeAcceptHandler(code);

            QnMutexLocker lock(&m_mutex);
            if (m_stopHandler)
                return;

            auto socket = std::move(m_udtConnectionSocket);
            socket.reset();

            lock.unlock();
            executeAcceptHandler(
                SystemError::noError,
                std::make_unique<IncomingTunnelUdtConnection>(
                    m_connectionId, std::move(socket)));
        });
}

} // namespace cloud
} // namespace network
} // namespace nx
