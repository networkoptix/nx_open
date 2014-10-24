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


class StunServerConnection;
extern template class StreamConnectionHolder<StunServerConnection>;

class StunServerConnection
:
    public nx_api::BaseStreamProtocolConnection<
        StunServerConnection,
        StreamConnectionHolder<StunServerConnection>,
        nx_stun::Message,
        nx_stun::MessageParser,
        nx_stun::MessageSerializer>
{
public:
    typedef BaseStreamProtocolConnection<
        StunServerConnection,
        StreamConnectionHolder<StunServerConnection>,
        nx_stun::Message,
        nx_stun::MessageParser,
        nx_stun::MessageSerializer
    > BaseType;

    StunServerConnection(
        StreamConnectionHolder<StunServerConnection>* socketServer,
        AbstractCommunicatingSocket* sock );

    void processMessage( nx_stun::Message&& request );

private:
    void processGetIPAddressRequest( nx_stun::Message&& request );
    void processProprietaryRequest( nx_stun::Message&& request );
    void sendErrorReply( const nx_stun::TransactionID& transaciton_id , nx_stun::ErrorCode::Type errorCode );
    void sendSuccessReply( const nx_stun::TransactionID& transaction_id );

    // Verification routine for Bind session
    // TODO:: Once these steps clear, we can add verification function for each
    // attributes. 

    bool verifyServerName( const nx_stun::attr::StringAttributeType& name );
    bool verifyServerId( const nx_stun::attr::StringAttributeType& name );
    bool verifyAuthroization( const nx_stun::attr::StringAttributeType& name );

    void onSendComplete( SystemError::ErrorCode ec ) {
        Q_UNUSED(ec);
        // After sending the request, then we start to read the response
        bool ret = BaseType::startReadingConnection();
        Q_ASSERT(ret);
    }

private:
    SocketAddress peer_address_;
};

#endif  //STUN_SERVER_CONNECTION_H
