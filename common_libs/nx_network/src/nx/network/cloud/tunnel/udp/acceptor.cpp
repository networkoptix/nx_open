
#include "acceptor.h"

#include <utils/serialization/lexical.h>

#include "incoming_tunnel_connection.h"


namespace nx {
namespace network {
namespace cloud {
namespace udp {

TunnelAcceptor::TunnelAcceptor(
    SocketAddress peerAddress,
    nx::hpm::api::ConnectionParameters connectionParametes)
:
    m_peerAddress(std::move(peerAddress)),
    m_connectionParameters(std::move(connectionParametes)),
    m_udpRetransmissionTimeout(stun::UDPClient::kDefaultRetransmissionTimeOut),
    m_udpMaxRetransmissions(stun::UDPClient::kDefaultMaxRetransmissions)
{
}

void TunnelAcceptor::setUdpRetransmissionTimeout(
    std::chrono::milliseconds timeout)
{
    m_udpRetransmissionTimeout = timeout;
}

void TunnelAcceptor::setUdpMaxRetransmissions(int count)
{
    m_udpMaxRetransmissions = count;
}

void TunnelAcceptor::accept(std::function<void(
    SystemError::ErrorCode,
    std::unique_ptr<AbstractIncomingTunnelConnection>)> handler)
{
    NX_ASSERT(!m_acceptHandler);
    NX_ASSERT(!m_udpMediatorConnection);
    m_mediatorConnection->dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            m_acceptHandler = std::move(handler);
            m_udpMediatorConnection = std::make_unique<
                hpm::api::MediatorServerUdpConnection>(
                    m_mediatorConnection->remoteAddress(),
                    m_mediatorConnection->credentialsProvider());

            m_udpMediatorConnection->socket()->bindToAioThread(
                m_mediatorConnection->getAioThread());

            hpm::api::ConnectionAckRequest ackRequest;
            ackRequest.connectSessionId = m_connectionId;
            ackRequest.connectionMethods = hpm::api::ConnectionMethod::udpHolePunching;

            m_udpMediatorConnection->setRetransmissionTimeOut(m_udpRetransmissionTimeout);
            m_udpMediatorConnection->setMaxRetransmissions(m_udpMaxRetransmissions);
            m_udpMediatorConnection->connectionAck(
                std::move(ackRequest),
                [this](nx::hpm::api::ResultCode code) { connectionAckResult(code); });
        });
}

void TunnelAcceptor::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorConnection->pleaseStop(
        [this, handler = std::move(handler)]()
        {
            if (m_udtConnectionSocket)
                m_udtConnectionSocket->pleaseStopSync();

            if (m_udpMediatorConnection)
                m_udpMediatorConnection->pleaseStopSync();

            handler();
        });
}

void TunnelAcceptor::connectionAckResult(
    nx::hpm::api::ResultCode code)
{
    NX_ASSERT(m_mediatorConnection->isInSelfAioThread());
    if (code != hpm::api::ResultCode::ok)
    {
        NX_LOGX(lm("connectionAck error: %1")
            .arg(QnLexical::serialized(code)), cl_logWARNING);

        // TODO: #mux code translation
        return executeAcceptHandler(SystemError::connectionAbort);
    }

    auto socket = m_udpMediatorConnection->takeSocket();
    m_udpMediatorConnection.reset();

    m_udtConnectionSocket.reset(new UdtStreamSocket);
    m_udtConnectionSocket->bindToAioThread(
        m_mediatorConnection->getAioThread());

    if (!m_udtConnectionSocket->bindToUdpSocket(std::move(*socket)) ||
        !m_udtConnectionSocket->setRendezvous(true) ||
        !m_udtConnectionSocket->setSendTimeout(
            m_connectionParameters.rendezvousConnectTimeout.count()) ||
        !m_udtConnectionSocket->setNonBlockingMode(true))
    {
        return executeAcceptHandler(
            SystemError::getLastOSErrorCode());
    }

    initiateUdtConnection();
}

void TunnelAcceptor::initiateUdtConnection()
{
    NX_ASSERT(m_mediatorConnection->isInSelfAioThread());
    NX_LOGX(lm("Initiate rendevous UDT connection from %1 to %2, "
        "connectionId=%3, remotePeerId=%4")
        .arg(m_udtConnectionSocket->getLocalAddress().toString())
        .arg(m_peerAddress.toString())
        .arg(m_connectionId).arg(m_remotePeerId), cl_logDEBUG2);

    m_udtConnectionSocket->connectAsync(
        m_peerAddress,
        [this](SystemError::ErrorCode code)
        {
            NX_LOGX(lm("Rendezvous UDT connection from %1 to %2 result: %3, "
                "connectionId=%4, remotePeerId=%5")
                .arg(m_udtConnectionSocket->getLocalAddress().toString())
                .arg(m_peerAddress.toString()).arg(SystemError::toString(code))
                .arg(m_connectionId).arg(m_remotePeerId), cl_logDEBUG1);

            if (code != SystemError::noError)
                return executeAcceptHandler(code);

            auto socket = std::move(m_udtConnectionSocket);
            m_udtConnectionSocket = nullptr;

            executeAcceptHandler(
                SystemError::noError,
                std::make_unique<IncomingTunnelConnection>(
                    m_connectionId,
                    std::move(socket),
                    m_connectionParameters));
        });
}

void TunnelAcceptor::executeAcceptHandler(
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    NX_ASSERT(m_mediatorConnection->isInSelfAioThread());
    NX_ASSERT(m_acceptHandler);

    const auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;
    return handler(code, std::move(connection));
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
