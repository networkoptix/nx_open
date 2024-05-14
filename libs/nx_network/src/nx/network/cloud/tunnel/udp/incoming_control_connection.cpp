// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incoming_control_connection.h"

#include <nx/network/cloud/data/tunnel_connection_chosen_data.h>
#include <nx/network/cloud/data/udp_hole_punching_connection_initiation_data.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/utils/log/log.h>

namespace nx::network::cloud::udp {

static constexpr int kBufferSize = 4 * 1024;

IncomingControlConnection::IncomingControlConnection(
    std::string connectionId,
    std::unique_ptr<AbstractStreamSocket> socket,
    const nx::hpm::api::ConnectionParameters& connectionParameters)
    :
    m_connectionId(std::move(connectionId)),
    m_socket(std::move(socket)),
    m_maxKeepAliveInterval(
        connectionParameters.udpTunnelKeepAliveInterval *
        connectionParameters.udpTunnelKeepAliveRetries),
    m_lastKeepAliveTime(std::chrono::steady_clock::now())
{
    m_recvBuffer.reserve(kBufferSize);
    m_sendBuffer.reserve(kBufferSize);
    m_parser.setMessage(&m_message);

    bindToAioThread(getAioThread());
}

void IncomingControlConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_socket->bindToAioThread(aioThread);
}

void IncomingControlConnection::setErrorHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    NX_ASSERT(m_socket->isInSelfAioThread());
    m_errorHandler = std::move(handler);
}

void IncomingControlConnection::start(
    nx::utils::MoveOnlyFunc<void()> selectedHandler)
{
    NX_ASSERT(m_socket->isInSelfAioThread());
    m_selectedHandler = std::move(selectedHandler);
    monitorKeepAlive(std::chrono::steady_clock::now());
    readConnectionRequest();
}

void IncomingControlConnection::resetLastKeepAlive()
{
    m_lastKeepAliveTime = std::chrono::steady_clock::now();
    NX_VERBOSE(this, nx::format("Update last keep alive"));
}

void IncomingControlConnection::stopWhileInAioThread()
{
    m_socket.reset();
}

AbstractStreamSocket* IncomingControlConnection::socket()
{
    return m_socket.get();
}

std::unique_ptr<AbstractStreamSocket> IncomingControlConnection::takeSocket()
{
    auto sock = std::exchange(m_socket, nullptr);
    sock->cancelRead();
    sock->cancelWrite();
    return sock;
}

void IncomingControlConnection::monitorKeepAlive(
    std::chrono::steady_clock::time_point currentTime)
{
    using namespace std::chrono;

    if (m_maxKeepAliveInterval == kNoTimeout)
        return;

    const auto delay = std::max(
        floor<milliseconds>(m_lastKeepAliveTime + m_maxKeepAliveInterval - currentTime),
        milliseconds(1));

    NX_VERBOSE(this, "Set keep alive timer for %1", delay);

    m_socket->registerTimer(
        delay,
        [this]()
        {
            const auto currentTime = steady_clock::now();
            if (m_lastKeepAliveTime + m_maxKeepAliveInterval < currentTime)
                return reportError(SystemError::timedOut);

            monitorKeepAlive(currentTime);
        });
}

void IncomingControlConnection::readConnectionRequest()
{
    m_message = stun::Message();
    m_parser.reset();
    continueReadRequest();
}

void IncomingControlConnection::continueReadRequest()
{
    m_recvBuffer.resize(0);
    m_socket->readSomeAsync(
        &m_recvBuffer,
        [this](SystemError::ErrorCode code, size_t bytesRead)
        {
            NX_ASSERT(code != SystemError::timedOut);
            if (code != SystemError::noError)
                return reportError(code);

            if (bytesRead == 0)
                return reportError(SystemError::connectionReset);

            resetLastKeepAlive();
            size_t processed = 0;
            switch(m_parser.parse(m_recvBuffer, &processed))
            {
                case nx::network::server::ParserState::init:
                case nx::network::server::ParserState::readingMessage:
                    return continueReadRequest();

                case nx::network::server::ParserState::done:
                    return processRequest();

                case nx::network::server::ParserState::failed:
                    return reportError(SystemError::invalidData);

                case nx::network::server::ParserState::readingBody:
                    // Stun message cannot have body.
                    NX_ASSERT(false);
                    return reportError(SystemError::invalidData);
            };
        });
}

void IncomingControlConnection::processRequest()
{
    stun::Message response(stun::Header(
        stun::MessageClass::successResponse, 0,
        std::move(m_message.header.transactionId)));
    if (m_message.header.method == 0)
        return; //< Work-around for a bug in an older version.

    if (!tryProcess<
            hpm::api::UdpHolePunchingSynRequest,
            hpm::api::UdpHolePunchingSynResponse>(&response) &&
        !tryProcess<
            hpm::api::TunnelConnectionChosenRequest,
            hpm::api::TunnelConnectionChosenResponse>(&response))
    {
        // TODO: #muskov Provide better error handling
        response.header.messageClass = stun::MessageClass::errorResponse;
        response.header.method = m_message.header.method;
        response.newAttribute<stun::attrs::ErrorCode>(
            stun::error::badRequest, std::string("Unsupported"));
    }

    stun::MessageSerializer serializer;
    serializer.setMessage(&response);

    size_t written = 0;
    m_sendBuffer.resize(0);
    serializer.serialize(&m_sendBuffer, &written);

    m_socket->sendAsync(
        &m_sendBuffer,
        [this]( SystemError::ErrorCode code, size_t)
        {
            if (code != SystemError::noError)
                return reportError(code);

            resetLastKeepAlive();
            readConnectionRequest();
        });
}

void IncomingControlConnection::reportError(SystemError::ErrorCode code)
{
    if (m_errorHandler)
        nx::utils::swapAndCall(m_errorHandler, code);
}

hpm::api::UdpHolePunchingSynResponse IncomingControlConnection::process(
    hpm::api::UdpHolePunchingSynRequest /*syn*/)
{
    NX_DEBUG(this, "Send SYN+ACK for connection %1", m_connectionId);

    hpm::api::UdpHolePunchingSynResponse synAck;
    synAck.connectSessionId = m_connectionId;

    notifyOnConnectionSelectedIfNeeded();

    return synAck;
}

hpm::api::TunnelConnectionChosenResponse IncomingControlConnection::process(
    hpm::api::TunnelConnectionChosenRequest /*request*/)
{
    NX_DEBUG(this, "Connection %1 has been chosen", m_connectionId);

    notifyOnConnectionSelectedIfNeeded();

    return hpm::api::TunnelConnectionChosenResponse();
}

void IncomingControlConnection::notifyOnConnectionSelectedIfNeeded()
{
    if (m_selectedHandler)
        nx::utils::moveAndCall(m_selectedHandler);
}

} // namespace nx::network::cloud::udp
