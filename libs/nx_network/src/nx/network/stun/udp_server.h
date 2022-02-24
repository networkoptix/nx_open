// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/server_statistics.h>
#include <nx/utils/counter.h>
#include <nx/utils/system_error.h>

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"
#include "unreliable_message_pipeline.h"

namespace nx {
namespace network {
namespace stun {

class MessageDispatcher;

/**
 * Receives STUN message over udp, forwards them to dispatcher, sends response message.
 * NOTE: Class methods are not thread-safe.
 */
class NX_NETWORK_API UdpServer:
    public network::aio::BasicPollable,
    public network::server::AbstractStatisticsProvider,
    private nx::network::UnreliableMessagePipelineEventHandler<Message>
{
    typedef nx::network::UnreliableMessagePipeline<
        Message,
        MessageParser,
        MessageSerializer> PipelineType;

public:
    UdpServer(const MessageDispatcher* dispatcher);
    virtual ~UdpServer() override;

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    bool bind(const SocketAddress& localAddress);
    /**
     * Start receiving messages.
     * If UdpServer::bind has not been called, random local address is occupied.
     * @param backlogSize Ignored. Introduce to make API compatible with tcp servers.
     */
    bool listen(int backlogSize = 0);

    void stopReceivingMessagesSync();

    /**
     * Returns local address occupied by server.
     */
    SocketAddress address() const;

    void sendMessage(
        SocketAddress destinationEndpoint,
        const Message& message,
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> completionHandler);

    const std::unique_ptr<AbstractDatagramSocket>& socket();

    void waitUntilAllRequestsCompleted();

    /**
     * If set to true then received messages with no FINGERPRINT are ignored.
     * By default, false.
     */
    void setFingerprintRequired(bool val);

    virtual nx::network::server::Statistics statistics() const override;

private:
    PipelineType m_messagePipeline;
    bool m_boundToLocalAddress = false;
    const MessageDispatcher* m_dispatcher = nullptr;
    bool m_fingerprintRequired = false;

    /**
     * Using shared_ptr since some usages may remove server before completion
     * of every running request.
     */
    std::shared_ptr<nx::utils::Counter> m_activeRequestCounter;

    virtual void stopWhileInAioThread() override;

    virtual void messageReceived(SocketAddress sourceAddress, Message mesage) override;
    virtual void ioFailure(SystemError::ErrorCode) override;
};

} // namespace stun
} // namespace network
} // namespace nx
