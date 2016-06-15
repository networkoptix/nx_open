
#include "acceptor.h"

#include <utils/serialization/lexical.h>

#include "incoming_tunnel_connection.h"


namespace nx {
namespace network {
namespace cloud {
namespace udp {

TunnelAcceptor::TunnelAcceptor(
    std::list<SocketAddress> peerAddresses,
    nx::hpm::api::ConnectionParameters connectionParametes)
:
    m_peerAddresses(std::move(peerAddresses)),
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
            m_udpMediatorConnection->socket()->bind(SocketAddress::anyAddress);

            hpm::api::ConnectionAckRequest ackRequest;
            ackRequest.connectSessionId = m_connectionId;
            ackRequest.connectionMethods = hpm::api::ConnectionMethod::udpHolePunching;

            const auto port = m_udpMediatorConnection->localAddress().port;
            for (const auto ifInfo: getAllIPv4Interfaces())
            {
                SocketAddress boundAddress(ifInfo.address.toString(), port);
                ackRequest.udpEndpointList.push_back(std::move(boundAddress));
            }

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
            m_udpMediatorConnection.reset();
            m_sockets.clear();
            m_connections.clear();
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

    auto udpSocket = m_udpMediatorConnection->takeSocket();
    m_udpMediatorConnection.reset();

    auto localAddress = udpSocket->getLocalAddress();
    auto timeout = m_connectionParameters.rendezvousConnectTimeout.count();
    for (const auto& address : m_peerAddresses)
    {
        auto udtSocket = std::make_unique<UdtStreamSocket>();
        udtSocket->bindToAioThread(m_mediatorConnection->getAioThread());

        if ((udpSocket
                ? udtSocket->bindToUdpSocket(std::move(*udpSocket))
                : udtSocket->bind(localAddress)) &&
            udtSocket->setRendezvous(true) &&
            udtSocket->setSendTimeout(timeout) &&
            udtSocket->setNonBlockingMode(true))
        {
            udpSocket.reset();
            auto it = m_sockets.emplace(m_sockets.end(), std::move(udtSocket));
            startUdtConnection(it, address);
        }
        else
        {
            return executeAcceptHandler( SystemError::getLastOSErrorCode());
        }
    }
}

void TunnelAcceptor::startUdtConnection(
    std::list<std::unique_ptr<UdtStreamSocket>>::iterator socketIt,
    const SocketAddress& target)
{
    NX_ASSERT(m_mediatorConnection->isInSelfAioThread());
    NX_LOGX(lm("Initiate rendevous UDT connection from %1 to %2, "
        "connectionId=%3, remotePeerId=%4")
        .str((*socketIt)->getLocalAddress()).str(target)
        .arg(m_connectionId).arg(m_remotePeerId), cl_logDEBUG2);

    (*socketIt)->connectAsync(
        target,
        [this, socketIt, target](SystemError::ErrorCode code)
        {
            auto socket = std::move(*socketIt);
            m_sockets.erase(socketIt);

            NX_LOGX(lm("Rendezvous UDT connection from %1 to %2 result: %3, "
                "connectionId=%4, remotePeerId=%5")
                .str(socket->getLocalAddress()).str(target).arg(SystemError::toString(code))
                .arg(m_connectionId).arg(m_remotePeerId), cl_logDEBUG1);

            if (code != SystemError::noError)
                return executeAcceptHandler(code);

            auto connectionIt = m_connections.emplace(
                m_connections.end(),
                std::make_unique<IncommingControlConnection>(
                    m_connectionId, std::move(socket), m_connectionParameters));

            (*connectionIt)->setErrorHandler(
                [this, connectionIt](SystemError::ErrorCode code)
                {
                    m_connections.erase(connectionIt);
                    executeAcceptHandler(code);
                });

            (*connectionIt)->start(
                [this, connectionIt]()
                {
                    auto tunnel = std::make_unique<IncomingTunnelConnection>(
                        std::move(*connectionIt));

                    m_connections.erase(connectionIt);
                    executeAcceptHandler(SystemError::noError, std::move(tunnel));
                });
        });
}

void TunnelAcceptor::executeAcceptHandler(
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    NX_ASSERT(m_mediatorConnection->isInSelfAioThread());
    NX_ASSERT(m_acceptHandler);

    if (code != SystemError::noError && (!m_sockets.empty() || !m_connections.empty()))
    {
        // It is to early to give up, while some connections are in progress
        // let's wait for more results
        return;
    }

    const auto handler = std::move(m_acceptHandler);
    m_acceptHandler = nullptr;
    return handler(code, std::move(connection));
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
