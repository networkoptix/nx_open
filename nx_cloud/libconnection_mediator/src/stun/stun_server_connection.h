/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_SERVER_CONNECTION_H
#define STUN_SERVER_CONNECTION_H

#include <functional>

#include <utils/common/stoppable.h>
#include <utils/network/stun/stun_message.h>
#include <utils/network/stun/stun_message_parser.h>
#include <utils/network/stun/stun_message_parse_handler.h>
#include <utils/network/stun/stun_message_serializer.h>
#include <utils/network/connection_server/base_stream_protocol_connection.h>
#include <utils/network/connection_server/stream_socket_server.h>

namespace nx {
namespace hpm {

class StunServerConnection
:
    public nx_api::BaseStreamProtocolConnection<
        StunServerConnection,
        StreamConnectionHolder<StunServerConnection>,
        stun::Message,
        stun::MessageParser,
        stun::MessageSerializer>
{
public:
    typedef BaseStreamProtocolConnection<
        StunServerConnection,
        StreamConnectionHolder<StunServerConnection>,
        stun::Message,
        stun::MessageParser,
        stun::MessageSerializer
    > BaseType;

    StunServerConnection(
        StreamConnectionHolder<StunServerConnection>* socketServer,
        std::unique_ptr<AbstractCommunicatingSocket> sock );

    void processMessage( stun::Message&& message );

private:
    void processBindingRequest( stun::Message&& request );
    void processCustomRequest( stun::Message&& request );

private:
    SocketAddress peer_address_;
};

} // namespace hpm
} // namespace nx

#endif  //STUN_SERVER_CONNECTION_H
