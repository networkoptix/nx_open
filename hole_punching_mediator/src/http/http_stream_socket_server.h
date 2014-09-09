/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_STREAM_SOCKET_SERVER_H
#define HTTP_STREAM_SOCKET_SERVER_H

#include <utils/network/connection_server/stream_socket_server.h>

#include "http_server_connection.h"


namespace nx_http
{
    class HttpStreamSocketServer
    :
        public StreamSocketServer<HttpStreamSocketServer, HttpServerConnection>
    {
        typedef StreamSocketServer<HttpStreamSocketServer, HttpServerConnection> base_type;

    public:
        typedef HttpServerConnection ConnectionType;

        HttpStreamSocketServer( bool sslRequired, SocketFactory::NatTraversalType natTraversalRequired )
        :
            base_type( sslRequired, natTraversalRequired )
        {
        }
    };
}

#endif  //HTTP_STREAM_SOCKET_SERVER_H
