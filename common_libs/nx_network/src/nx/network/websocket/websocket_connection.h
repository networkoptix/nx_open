#pragma once

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/websocket/websocket_message_parser.h>
#include <nx/network/websocket/websocket_message_serializer.h>
#include <nx/network/websocket/websocket_message.h>

namespace nx {
namespace network {
namespace websocket {

class Connection :
	public nx_api::BaseStreamProtocolConnection<
		Connection,
		Message,
		MessageParser,
		MessageSerializer
	>
{
    using BaseType = nx_api::BaseStreamProtocolConnection<
        Connection,
        Message,
        MessageParser,
        MessageSerializer>;
    
public:
    Connection(
        StreamConnectionHolder<Connection>* socketServer, 
        std::unique_ptr<AbstractStreamSocket> sock,
        int version,
        bool isSecure,
        bool isServer);
    virtual ~Connection();
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    void processMessage(Message&& request);

private:
private:
    ProcessorBaseTypePtr m_processor;
    ConnectionMessageManagerTypePtr<MessageType> m_conMessageManager;
    RandomGenType m_randomGen;
};

}
}
}