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

    void processMessage( stun::Message&& request );

private:
    void processGetIPAddressRequest( stun::Message&& request );
    void processProprietaryRequest( stun::Message&& request );
    void sendErrorReply( const stun::TransactionID& transaciton_id,
                         stun::ErrorCode::Type errorCode,\
                         nx::String description );
    void sendSuccessReply( const stun::TransactionID& transaction_id );

    // Verification routine for Bind session
    // TODO:: Once these steps clear, we can add verification function for each
    // attributes. 

    bool verifySystemName( const String& name );
    bool verifyServerId( const QnUuid& id );
    bool verifyAuthroization( const String& auth );

    void onSendComplete( SystemError::ErrorCode ec ) {
        Q_UNUSED(ec);
        // After sending the request, then we start to read the response
        bool ret = BaseType::startReadingConnection();
        Q_ASSERT(ret);
    }

private:
    SocketAddress peer_address_;
};

} // namespace hpm
} // namespace nx

#endif  //STUN_SERVER_CONNECTION_H
