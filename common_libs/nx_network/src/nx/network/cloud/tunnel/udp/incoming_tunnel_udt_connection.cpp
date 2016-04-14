#include "incoming_tunnel_udt_connection.h"

#include <nx/network/cloud/cloud_config.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

IncomingTunnelUdtConnection::IncomingTunnelUdtConnection(
    String connectionId,
    std::unique_ptr<UdtStreamSocket> connectionSocket,
    std::chrono::milliseconds maxKeepAliveInterval)
:
    AbstractIncomingTunnelConnection(std::move(connectionId)),
    m_maxKeepAliveInterval(
        maxKeepAliveInterval == std::chrono::milliseconds::zero()
        ? std::chrono::duration_cast<std::chrono::milliseconds>(
            kHpUdtKeepAliveInterval * kHpUdtKeepAliveRetries)
        : maxKeepAliveInterval),
    m_lastKeepAlive(std::chrono::steady_clock::now()),
    m_state(SystemError::noError),
    m_connectionSocket(std::move(connectionSocket)),
    m_serverSocket(new UdtStreamServerSocket)
{
    m_serverSocket->bindToAioThread(m_connectionSocket->getAioThread());
    if (!m_serverSocket->setNonBlockingMode(true) ||
        !m_serverSocket->bind(m_connectionSocket->getLocalAddress()) ||
        !m_serverSocket->listen())
    {
        NX_LOGX(lm("Can not listen on server socket: ")
            .arg(SystemError::getLastOSErrorText()), cl_logWARNING);

        m_state = SystemError::getLastOSErrorCode();
    }
    else
    {
        NX_LOGX(lm("Listening for new connections on %1")
            .arg(m_serverSocket->getLocalAddress().toString()), cl_logDEBUG1);

        m_connectionBuffer.reserve(1024);
        m_connectionParser.setMessage(&m_connectionMessage);
        monitorKeepAlive();
        readConnectionRequest();
    }
}

void IncomingTunnelUdtConnection::accept(std::function<void(
    SystemError::ErrorCode,
    std::unique_ptr<AbstractStreamSocket>)> handler)
{
    NX_ASSERT(!m_acceptHandler, Q_FUNC_INFO, "Concurrent accept");
    m_connectionSocket->post(
        [this, handler = std::move(handler)]()
        {
            if (m_state != SystemError::noError)
                return handler(m_state, nullptr);

            m_acceptHandler = std::move(handler);
            m_serverSocket->acceptAsync(
                [this](SystemError::ErrorCode code,
                       AbstractStreamSocket* socket)
            {
                m_lastKeepAlive = std::chrono::steady_clock::now();
                NX_LOGX(lm("Accepted %1 (%2)")
                    .arg(socket).arg(SystemError::toString(code)), cl_logDEBUG2);

                if (code != SystemError::noError)
                {
                    m_state = code;
                    m_serverSocket.reset();
                }

                const auto handler = std::move(m_acceptHandler);
                m_acceptHandler = nullptr;
                handler(
                    SystemError::noError,
                    std::unique_ptr<AbstractStreamSocket>(socket));
            });
        });
}

void IncomingTunnelUdtConnection::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_connectionSocket->pleaseStop(
        [this, handler = std::move(handler)]()
        {
            if (m_serverSocket)
                m_serverSocket->pleaseStopSync(); // we are in IO thread

            handler();
        });
}

void IncomingTunnelUdtConnection::monitorKeepAlive()
{
    using namespace std::chrono;
    auto timePassed = steady_clock::now() - m_lastKeepAlive;
    if (timePassed >= m_maxKeepAliveInterval)
        return connectionSocketError(SystemError::timedOut);

    auto next = duration_cast<milliseconds>(m_maxKeepAliveInterval - timePassed);
    m_connectionSocket->registerTimer(next, [this](){ monitorKeepAlive(); });
}

void IncomingTunnelUdtConnection::readConnectionRequest()
{
    m_connectionParser.reset();
    readRequest();
}

void IncomingTunnelUdtConnection::readRequest()
{
    m_connectionBuffer.resize(0);
    m_connectionSocket->readSomeAsync(
        &m_connectionBuffer,
        [this](SystemError::ErrorCode code, size_t bytesRead)
        {
            NX_ASSERT(code != SystemError::timedOut);
            if (code != SystemError::noError)
                return connectionSocketError(code);

            if (bytesRead == 0)
                return connectionSocketError(SystemError::connectionReset);

            m_lastKeepAlive = std::chrono::steady_clock::now();
            size_t processed = 0;
            switch(m_connectionParser.parse(
                m_connectionBuffer, &processed))
            {
                case nx_api::ParserState::init:
                case nx_api::ParserState::inProgress:
                    return readRequest();

                case nx_api::ParserState::done:
                {
                    hpm::api::UdpHolePunchingSyn syn;
                    if (!syn.parse(m_connectionMessage))
                        return connectionSocketError(SystemError::invalidData);

                    return writeResponse();
                }

                case nx_api::ParserState::failed:
                    return connectionSocketError(SystemError::invalidData);
            };
        });
}


void IncomingTunnelUdtConnection::writeResponse()
{
    NX_LOGX(lm("Send SYN+ACK for connection %1")
        .arg(m_connectionId), cl_logDEBUG1);

    hpm::api::UdpHolePunchingSynAck synAck;
    synAck.connectSessionId = m_connectionId;

    stun::Message message(
        stun::Header(
            stun::MessageClass::successResponse,
            stun::cc::methods::udpHolePunchingSynAck,
            m_connectionMessage.header.transactionId));
    synAck.serialize(&message);

    stun::MessageSerializer serializer;
    serializer.setMessage(&message);

    size_t written;
    m_connectionBuffer.resize(0);
    serializer.serialize(&m_connectionBuffer, &written);

    m_connectionSocket->sendAsync(
        m_connectionBuffer,
        [this]( SystemError::ErrorCode code, size_t)
        {
            if (code != SystemError::noError)
                return connectionSocketError(code);

            m_lastKeepAlive = std::chrono::steady_clock::now();
            readConnectionRequest();
        });
}

void IncomingTunnelUdtConnection::connectionSocketError(
    SystemError::ErrorCode code)
{
    NX_LOGX(lm("Connection socket error (%1), closing tunnel...")
        .arg(SystemError::toString(code)), cl_logWARNING);

    m_state = code;
    if (m_serverSocket)
    {
        m_serverSocket->pleaseStopSync(); // we are in IO thread
        m_serverSocket.reset();
    }

    if (m_acceptHandler)
    {
        const auto handler = std::move(m_acceptHandler);
        m_acceptHandler = nullptr;
        return handler(code, nullptr);
    }
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
