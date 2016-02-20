/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <utils/common/systemerror.h>

#include "nx/network/socket_factory.h"

#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"
#include "unreliable_message_pipeline.h"


namespace nx {
namespace stun {

class MessageDispatcher;

/** Receives STUN message over udp, forwards them to dispatcher, sends response message.
    \note Class methods are not thread-safe
*/
class NX_NETWORK_API UDPServer
:
    public QnStoppableAsync,
    private nx::network::UnreliableMessagePipelineEventHandler<Message>
{
    typedef nx::network::UnreliableMessagePipeline<
        Message,
        MessageParser,
        MessageSerializer> PipelineType;

public:
    UDPServer(const MessageDispatcher& dispatcher);
    virtual ~UDPServer();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    bool bind(const SocketAddress& localAddress);
    /** Start receiving messages.
        If \a UDPServer::bind has not been called, random local address is occupied
    */
    bool listen();

    /** Returns local address occupied by server */
    SocketAddress address() const;

    void sendMessage(
        SocketAddress destinationEndpoint,
        const Message& message,
        std::function<void(SystemError::ErrorCode)> completionHandler);
    const std::unique_ptr<network::UDPSocket>& socket();

private:
    PipelineType m_messagePipeline;
    bool m_boundToLocalAddress;
    const MessageDispatcher& m_dispatcher;

    virtual void messageReceived(SocketAddress sourceAddress, Message mesage) override;
    virtual void ioFailure(SystemError::ErrorCode) override;
};

}   //stun
}   //nx
