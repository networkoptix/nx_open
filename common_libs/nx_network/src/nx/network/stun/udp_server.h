/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <deque>
#include <functional>

#include <utils/common/systemerror.h>

#include "nx/network/socket_common.h"
#include "nx/network/socket_factory.h"

#include "abstract_server_connection.h"
#include "message.h"
#include "message_parser.h"
#include "message_serializer.h"


namespace nx {
namespace stun {

class MessageDispatcher;

/** Receives STUN message over udp, forwards them to dispatcher, sends response message */
class NX_NETWORK_API UDPServer
{
public:
    UDPServer(
        const MessageDispatcher& dispatcher,
        SocketFactory::NatTraversalType natTraversalType);
    virtual ~UDPServer();

    bool bind(const SocketAddress&);
    bool listen();
    SocketAddress address() const;
    /**
        \note Message pipelining is supported
     */
    void sendMessageAsync(
        SocketAddress destinationEndpoint,
        Message message,
        std::function<void(SystemError::ErrorCode)> completionHandler);

private:
    struct OutgoingMessageContext
    {
        SocketAddress destinationEndpoint;
        Message message;
        std::function<void(SystemError::ErrorCode)> completionHandler;
    };

    const MessageDispatcher& m_dispatcher;
    std::unique_ptr<AbstractDatagramSocket> m_socket;
    nx::Buffer m_readBuffer;
    nx::Buffer m_writeBuffer;
    MessageParser m_messageParser;
    MessageSerializer m_messageSerializer;
    std::deque<OutgoingMessageContext> m_sendQueue;

    void onBytesRead(
        SystemError::ErrorCode errorCode,
        SocketAddress sourceAddress,
        size_t bytesRead);
    void sendMessageInternal(
        SocketAddress destinationEndpoint,
        Message message,
        std::function<void(SystemError::ErrorCode)> completionHandler);
    void sendOutNextMessage();
    void messageSent(SystemError::ErrorCode errorCode, size_t bytesSent);
};

}   //stun
}   //nx
