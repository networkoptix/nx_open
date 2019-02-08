#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/udt/udt_socket.h>

namespace nx {
namespace hpm {
namespace api {

class UdpHolePunchingSynRequest;
class UdpHolePunchingSynResponse;
class TunnelConnectionChosenRequest;
class TunnelConnectionChosenResponse;

} // namespace api
} // namespace hpm
} // namespace nx

namespace nx {
namespace network {
namespace cloud {
namespace udp {

class NX_NETWORK_API IncomingControlConnection:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    IncomingControlConnection(
        String connectionId,
        std::unique_ptr<UdtStreamSocket> socket,
        const nx::hpm::api::ConnectionParameters& connectionParameters);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * This handler is called when error accures (so object is not usefull any more).
     */
    void setErrorHandler(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    /**
     * Starts reading socket and prosesses events.
     * @param selectedHandler is called when this connection is selected by client.
     */
    void start(utils::MoveOnlyFunc<void()> selectedHandler);

    AbstractStreamSocket* socket();
    void resetLastKeepAlive();

private:
    virtual void stopWhileInAioThread() override;

    void monitorKeepAlive();
    void readConnectionRequest();
    void continueReadRequest();
    void processRequest();
    void handleError(SystemError::ErrorCode code);

    template<typename In, typename Out>
    bool tryProcess(stun::Message* response);

    hpm::api::UdpHolePunchingSynResponse process(hpm::api::UdpHolePunchingSynRequest syn);
    hpm::api::TunnelConnectionChosenResponse process(
        hpm::api::TunnelConnectionChosenRequest reqest);

    const String m_connectionId;
    std::unique_ptr<UdtStreamSocket> m_socket;
    const std::chrono::milliseconds m_maxKeepAliveInterval;

    std::chrono::steady_clock::time_point m_lastKeepAlive;
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_errorHandler;
    utils::MoveOnlyFunc<void()> m_selectedHandler;

    Buffer m_buffer;
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

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
