/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_STREAM_SOCKET_SERVER_H
#define HTTP_STREAM_SOCKET_SERVER_H

#include <utils/network/connection_server/stream_socket_server.h>

#include "abstract_authentication_manager.h"
#include "http_message_dispatcher.h"
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

        HttpStreamSocketServer(
            nx_http::AbstractAuthenticationManager* const authenticationManager,
            nx_http::MessageDispatcher* const httpMessageDispatcher,
            bool sslRequired,
            SocketFactory::NatTraversalType natTraversalRequired );

    protected:
        virtual std::shared_ptr<HttpServerConnection> createConnection(
            std::unique_ptr<AbstractStreamSocket> _socket) override;

    private:
        nx_http::AbstractAuthenticationManager* const m_authenticationManager;
        nx_http::MessageDispatcher* const m_httpMessageDispatcher;
    };
}

#endif  //HTTP_STREAM_SOCKET_SERVER_H
