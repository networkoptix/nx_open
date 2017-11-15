#pragma once

#include <nx/utils/system_error.h>

#include <nx/network/aio/basic_pollable.h>

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"
#include "unreliable_message_pipeline.h"

namespace nx {
namespace stun {

class MessageDispatcher;

/**
 * Receives STUN message over udp, forwards them to dispatcher, sends response message.
 * NOTE: Class methods are not thread-safe.
 */
class NX_NETWORK_API UdpServer:
    public network::aio::BasicPollable,
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
    const std::unique_ptr<network::UDPSocket>& socket();

private:
    PipelineType m_messagePipeline;
    bool m_boundToLocalAddress;
    const MessageDispatcher* m_dispatcher;

    virtual void stopWhileInAioThread() override;

    virtual void messageReceived(SocketAddress sourceAddress, Message mesage) override;
    virtual void ioFailure(SystemError::ErrorCode) override;
};

} // namespace stun
} // namespace nx
