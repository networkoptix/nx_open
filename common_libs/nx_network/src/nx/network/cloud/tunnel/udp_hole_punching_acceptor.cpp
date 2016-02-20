#include "udp_hole_punching_acceptor.h"

#include <nx/network/cloud/cloud_config.h>
#include <nx/network/cloud/tunnel/incoming_tunnel_udt_connection.h>
#include <utils/serialization/lexical.h>

namespace nx {
namespace network {
namespace cloud {

UdpHolePunchingTunnelAcceptor::UdpHolePunchingTunnelAcceptor(
    SocketAddress peerAddress)
:
    m_peerAddress(std::move(peerAddress)),
    m_udtConnectTimeout(kHpUdtConnectTimeout),
    m_udpRetransmissionTimeout(stun::UDPClient::kDefaultRetransmissionTimeOut),
    m_udpMaxRetransmissions(stun::UDPClient::kDefaultMaxRetransmissions)
{
}

void UdpHolePunchingTunnelAcceptor::setUdtConnectTimeout(
    std::chrono::milliseconds timeout)
{
    m_udtConnectTimeout = timeout;
}

void UdpHolePunchingTunnelAcceptor::setUdpRetransmissionTimeout(
    std::chrono::milliseconds timeout)
{
    m_udpRetransmissionTimeout = timeout;
}

void UdpHolePunchingTunnelAcceptor::setUdpMaxRetransmissions(int count)
{
    m_udpMaxRetransmissions = count;
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

    m_udpMediatorConnection->setRetransmissionTimeOut(m_udpRetransmissionTimeout);
    m_udpMediatorConnection->setMaxRetransmissions(m_udpMaxRetransmissions);
    m_udpMediatorConnection->connectionAck(
        std::move(ackRequest),
        [this](nx::hpm::api::ResultCode code) { connectionAckResult(code); });
}

void UdpHolePunchingTunnelAcceptor::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_stopHandler = std::move(handler);
    }

    auto callHandler = [this]()
    {
        const auto handler = std::move(m_stopHandler);
        handler();
    };

    auto stopUdtConnection = [this, callHandler = std::move(callHandler)]()
    {
        if (!m_udtConnectionSocket) return callHandler();
        m_udtConnectionSocket->pleaseStop(std::move(callHandler));
    };

    if (!m_udpMediatorConnection) return stopUdtConnection();
    m_udpMediatorConnection->pleaseStop(std::move(stopUdtConnection));
}

void UdpHolePunchingTunnelAcceptor::connectionAckResult(
    nx::hpm::api::ResultCode code)
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
        if (!m_udtConnectionSocket->bindToUdpSocket(std::move(*socket)) ||
            !m_udtConnectionSocket->setRendezvous(true) ||
            !m_udtConnectionSocket->setSendTimeout(
                m_udtConnectTimeout.count()) ||
            !m_udtConnectionSocket->setNonBlockingMode(true))
        {
            return executeAcceptHandler(
                SystemError::getLastOSErrorCode());
        }
    }

    initiateUdtConnection();
}

void UdpHolePunchingTunnelAcceptor::initiateUdtConnection()
{
    NX_LOGX(lm("Initiate rendevous UDT connection from %1 to %2")
        .arg(m_udtConnectionSocket->getLocalAddress().toString())
        .arg(m_peerAddress.toString()), cl_logDEBUG2);

    m_udtConnectionSocket->connectAsync(
        m_peerAddress,
        [this](SystemError::ErrorCode code)
        {
            NX_LOGX(lm("Randevous UDT connection from %1 to %2 result: %3")
                .arg(m_udtConnectionSocket->getLocalAddress().toString())
                .arg(m_peerAddress.toString())
                .arg(SystemError::toString(code)), cl_logDEBUG1);

            if (code != SystemError::noError)
                return executeAcceptHandler(code);

            QnMutexLocker lock(&m_mutex);
            if (m_stopHandler)
                return;

            auto socket = std::move(m_udtConnectionSocket);
            lock.unlock();

            executeAcceptHandler(
                SystemError::noError,
                std::make_unique<IncomingTunnelUdtConnection>(
                    m_connectionId, std::move(socket)));
        });
}

void UdpHolePunchingTunnelAcceptor::executeAcceptHandler(
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    const auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;

    Q_ASSERT(handler);
    return handler(code, std::move(connection));
}

} // namespace cloud
} // namespace network
} // namespace nx
