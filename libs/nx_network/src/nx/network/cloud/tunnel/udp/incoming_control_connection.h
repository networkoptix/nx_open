// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/udt/udt_socket.h>

namespace nx::hpm::api {

class UdpHolePunchingSynRequest;
class UdpHolePunchingSynResponse;
class TunnelConnectionChosenRequest;
class TunnelConnectionChosenResponse;

} // namespace nx::hpm::api

namespace nx::network::cloud::udp {

class NX_NETWORK_API IncomingControlConnection:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    IncomingControlConnection(
        std::string connectionId,
        std::unique_ptr<AbstractStreamSocket> socket,
        const nx::hpm::api::ConnectionParameters& connectionParameters);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * This handler is called when an error occurs (so the object is not useful any more).
     */
    void setErrorHandler(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    /**
     * Starts reading the socket and processes events.
     * @param selectedHandler is called when this connection is selected by client.
     */
    void start(utils::MoveOnlyFunc<void()> selectedHandler);

    AbstractStreamSocket* socket();
    std::unique_ptr<AbstractStreamSocket> takeSocket();

    void resetLastKeepAlive();

private:
    virtual void stopWhileInAioThread() override;

    void monitorKeepAlive(std::chrono::steady_clock::time_point currentTime);
    void readConnectionRequest();
    void continueReadRequest();
    void processRequest();
    void reportError(SystemError::ErrorCode code);

    template<typename In, typename Out>
    bool tryProcess(stun::Message* response);

    hpm::api::UdpHolePunchingSynResponse process(hpm::api::UdpHolePunchingSynRequest syn);
    hpm::api::TunnelConnectionChosenResponse process(hpm::api::TunnelConnectionChosenRequest reqest);
    void notifyOnConnectionSelectedIfNeeded();

private:
    const std::string m_connectionId;
    std::unique_ptr<AbstractStreamSocket> m_socket;
    const std::chrono::milliseconds m_maxKeepAliveInterval;

    std::chrono::steady_clock::time_point m_lastKeepAliveTime;
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_errorHandler;
    utils::MoveOnlyFunc<void()> m_selectedHandler;

    Buffer m_recvBuffer;
    Buffer m_sendBuffer;
    stun::Message m_message;
    stun::MessageParser m_parser;
};

template<typename In, typename Out>
bool IncomingControlConnection::tryProcess(stun::Message* response)
{
    if (m_message.header.method != In::kMethod)
        return false;

    In in;
    if (!in.parse(m_message))
        return false;

    auto out = process(std::move(in));
    response->header.method = Out::kMethod;
    out.serialize(response);
    return true;
}

} // namespace nx::network::cloud::udp
