#include "incoming_control_connection.h"

#include <nx/network/cloud/data/tunnel_connection_chosen_data.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {
namespace udp {

static const int kBufferSize(4 * 1024);

IncomingControlConnection::IncomingControlConnection(
    String connectionId,
    std::unique_ptr<UdtStreamSocket> socket,
    const nx::hpm::api::ConnectionParameters& connectionParameters)
    :
    m_connectionId(std::move(connectionId)),
    m_socket(std::move(socket)),
    m_maxKeepAliveInterval(
        connectionParameters.udpTunnelKeepAliveInterval *
        connectionParameters.udpTunnelKeepAliveRetries),
    m_lastKeepAlive(std::chrono::steady_clock::now())
{
    m_buffer.reserve(kBufferSize);
    m_parser.setMessage(&m_message);

    bindToAioThread(getAioThread());
}

void IncomingControlConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_socket->bindToAioThread(aioThread);
}

void IncomingControlConnection::setErrorHandler(
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
	NX_ASSERT(m_socket->isInSelfAioThread());
    m_errorHandler = std::move(handler);
}

void IncomingControlConnection::start(
    utils::MoveOnlyFunc<void()> selectedHandler)
{
	NX_ASSERT(m_socket->isInSelfAioThread());
    m_selectedHandler = std::move(selectedHandler);
    monitorKeepAlive();
    readConnectionRequest();
}

void IncomingControlConnection::resetLastKeepAlive()
{
    m_lastKeepAlive = std::chrono::steady_clock::now();
    NX_LOGX(lm("Update last keep alive"), cl_logDEBUG2);
}

void IncomingControlConnection::stopWhileInAioThread()
{
    m_socket.reset();
}

AbstractStreamSocket* IncomingControlConnection::socket()
{
    return m_socket.get();
}

void IncomingControlConnection::monitorKeepAlive()
{
    using namespace std::chrono;
    const auto timePassed = steady_clock::now() - m_lastKeepAlive;
    const auto next = duration_cast<milliseconds>(m_maxKeepAliveInterval - timePassed);
    if (next.count() <= 0)
        return handleError(SystemError::timedOut);

	NX_LOGX(lm("Set keep alive timer for %1 ms").arg(next.count()), cl_logDEBUG2);
    m_socket->registerTimer(next, [this](){ monitorKeepAlive(); });
}

void IncomingControlConnection::readConnectionRequest()
{
    m_parser.reset();
    continueReadRequest();
}

void IncomingControlConnection::continueReadRequest()
{
    m_buffer.resize(0);
    m_socket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode code, size_t bytesRead)
        {
            NX_EXPECT(code != SystemError::timedOut);
            if (code != SystemError::noError)
                return handleError(code);

            if (bytesRead == 0)
                return handleError(SystemError::connectionReset);

            resetLastKeepAlive();
            size_t processed = 0;
            switch(m_parser.parse(m_buffer, &processed))
            {
                case nx::network::server::ParserState::init:
                case nx::network::server::ParserState::readingMessage:
                    return continueReadRequest();

                case nx::network::server::ParserState::done:
                    return processRequest();

                case nx::network::server::ParserState::failed:
                    return handleError(SystemError::invalidData);

                case nx::network::server::ParserState::readingBody:
                    // Stun message cannot have body.
                    NX_ASSERT(false);
                    return handleError(SystemError::invalidData);
            };
        });
}

void IncomingControlConnection::processRequest()
{
    stun::Message response(stun::Header(
        stun::MessageClass::successResponse, 0,
        std::move(m_message.header.transactionId)));
    if (m_message.header.method == 0)
    {
        std::cerr<<"Received request with method 0 from "<<m_socket->getForeignAddress().toString().toStdString()<<std::endl;
        return; //protection from bug in old version
    }

    if (!tryProcess<
            hpm::api::UdpHolePunchingSynRequest,
            hpm::api::UdpHolePunchingSynResponse>(&response) &&
        !tryProcess<
            hpm::api::TunnelConnectionChosenRequest,
            hpm::api::TunnelConnectionChosenResponse>(&response))
    {
        // TODO: #mux Provide better error handling
        response.header.messageClass = stun::MessageClass::errorResponse;
        response.header.method = m_message.header.method;
        response.newAttribute<stun::attrs::ErrorCode>(
            stun::error::badRequest, String("Unsupported"));
    }

    stun::MessageSerializer serializer;
    serializer.setMessage(&response);

    size_t written;
    m_buffer.resize(0);
    serializer.serialize(&m_buffer, &written);

    m_socket->sendAsync(
        m_buffer,
        [this]( SystemError::ErrorCode code, size_t)
        {
            if (code != SystemError::noError)
                return handleError(code);

            resetLastKeepAlive();
            readConnectionRequest();
        });
}

void IncomingControlConnection::handleError(SystemError::ErrorCode code)
{
    const auto handler = std::move(m_errorHandler);
    if (handler)
        handler(code);
}

hpm::api::UdpHolePunchingSynResponse IncomingControlConnection::process(
    hpm::api::UdpHolePunchingSynRequest syn)
{
    static_cast<void>(syn);
    NX_LOGX(lm("Send SYN+ACK for connection %1")
        .arg(m_connectionId), cl_logDEBUG1);

    hpm::api::UdpHolePunchingSynResponse synAck;
    synAck.connectSessionId = m_connectionId;
    return synAck;
}

hpm::api::TunnelConnectionChosenResponse IncomingControlConnection::process(
    hpm::api::TunnelConnectionChosenRequest reqest)
{
    static_cast<void>(reqest);
    NX_LOGX(lm("Connection %1 has been chosen")
        .arg(m_connectionId), cl_logDEBUG1);

    auto handler = std::move(m_selectedHandler);
    m_selectedHandler = nullptr;

    handler();
    return hpm::api::TunnelConnectionChosenResponse();
}

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
