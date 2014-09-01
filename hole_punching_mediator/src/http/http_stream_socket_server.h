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
    public:
        typedef HttpServerConnection ConnectionType;
    };
}

#endif  //HTTP_STREAM_SOCKET_SERVER_H
