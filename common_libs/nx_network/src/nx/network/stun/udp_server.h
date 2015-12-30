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

/** Receives STUN message over udp, forwards them to dispatcher, sends response message */
class NX_NETWORK_API UDPServer
:
    public nx::network::UnreliableMessagePipeline<
        UDPServer,
        Message,
        MessageParser,
        MessageSerializer>
{
    typedef nx::network::UnreliableMessagePipeline<
        UDPServer,
        Message,
        MessageParser,
        MessageSerializer> base_type;

public:
    UDPServer(const MessageDispatcher& dispatcher);
    virtual ~UDPServer();

    bool listen();

    void messageReceived(SocketAddress sourceAddress, Message mesage);
    void ioFailure(SystemError::ErrorCode);

private:
    const MessageDispatcher& m_dispatcher;
};

}   //stun
}   //nx
