/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_SERVER_CONNECTION_H
#define STUN_SERVER_CONNECTION_H

#include "base_stream_protocol_connection.h"
#include "stun/stun_message.h"
#include "stun/stun_message_parser.h"
#include "stun/stun_message_parse_handler.h"
#include "stun/stun_message_serializer.h"


class StunStreamSocketServer;

class StunServerConnection
:
    public nx_api::BaseStreamProtocolConnection<
        StunServerConnection,
        StunStreamSocketServer,
        nx_stun::Message,
        nx_stun::MessageParser<nx_stun::MessageParseHandler>,
        nx_stun::MessageSerializer>
{
public:
    typedef BaseStreamProtocolConnection<
        StunServerConnection,
        StunStreamSocketServer,
        nx_stun::Message,
        nx_stun::MessageParser<nx_stun::MessageParseHandler>,
        nx_stun::MessageSerializer
    > BaseType;

    StunServerConnection(
        StunStreamSocketServer* socketServer,
        AbstractCommunicatingSocket* sock );

    void processMessage( const nx_stun::Message& request );
};

#endif  //STUN_SERVER_CONNECTION_H
