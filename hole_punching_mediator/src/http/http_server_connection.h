/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_SERVER_CONNECTION_H
#define HTTP_SERVER_CONNECTION_H

#include "base_stream_protocol_connection.h"

#include <utils/network/http/httptypes.h>
#include <utils/network/http/http_parser.h>
#include <utils/network/http/http_serializer.h>


namespace nx_http
{
    class HttpStreamSocketServer;
}

class HttpServerConnection
:
    public nx_api::BaseStreamProtocolConnection<
        HttpServerConnection,
        nx_http::HttpStreamSocketServer,
        nx_http::HttpMessage,
        nx_http::MessageParser,
        nx_http::MessageSerializer>
{
public:
    typedef BaseStreamProtocolConnection<
        HttpServerConnection,
        nx_http::HttpStreamSocketServer,
        nx_http::HttpMessage,
        nx_http::MessageParser,
        nx_http::MessageSerializer
    > BaseType;

    HttpServerConnection(
        nx_http::HttpStreamSocketServer* socketServer,
        AbstractCommunicatingSocket* sock );

    void processMessage( const nx_http::HttpMessage& request );
};

#endif  //HTTP_SERVER_CONNECTION_H
