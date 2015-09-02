/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_SERVER_CONNECTION_H
#define STUN_SERVER_CONNECTION_H

#include <functional>

#include <utils/common/stoppable.h>
#include <utils/network/stun/message.h>
#include <utils/network/stun/message_parser.h>
#include <utils/network/stun/message_serializer.h>
#include <utils/network/connection_server/base_stream_protocol_connection.h>
#include <utils/network/connection_server/stream_socket_server.h>
#include <utils/thread/mutex.h>

namespace nx {
namespace stun {

class ServerConnection
:
    public nx_api::BaseStreamProtocolConnection<
        stun::ServerConnection,
        StreamConnectionHolder<ServerConnection>,
        Message,
        MessageParser,
        MessageSerializer>,
    public std::enable_shared_from_this<ServerConnection>
{
public:
    typedef nx_api::BaseStreamProtocolConnection<
        ServerConnection,
        StreamConnectionHolder<ServerConnection>,
        Message,
        MessageParser,
        MessageSerializer
    > BaseType;

    ServerConnection(
        StreamConnectionHolder<ServerConnection>* socketServer,
        std::unique_ptr<AbstractCommunicatingSocket> sock );
    ~ServerConnection();

    void processMessage( Message message );
    void setDestructHandler( std::function< void() > handler = nullptr );

private:
    void processBindingRequest( Message message );
    void processCustomRequest( Message message );

private:
    const SocketAddress m_peerAddress;

    QnMutex m_mutex;
    std::function< void() > m_destructHandler;
};

} // namespace stun
} // namespace nx

#endif  //STUN_SERVER_CONNECTION_H
