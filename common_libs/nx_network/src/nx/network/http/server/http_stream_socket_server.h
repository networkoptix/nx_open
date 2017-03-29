/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_STREAM_SOCKET_SERVER_H
#define HTTP_STREAM_SOCKET_SERVER_H

#include <nx/network/connection_server/stream_socket_server.h>

#include "abstract_authentication_manager.h"
#include "http_message_dispatcher.h"
#include "http_server_connection.h"


namespace nx_http
{
class HttpStreamSocketServer:
    public StreamSocketServer<HttpStreamSocketServer, HttpServerConnection>
{
    typedef StreamSocketServer<HttpStreamSocketServer, HttpServerConnection> base_type;

public:
    typedef HttpServerConnection ConnectionType;

    HttpStreamSocketServer(
        nx_http::server::AbstractAuthenticationManager* const authenticationManager,
        nx_http::MessageDispatcher* const httpMessageDispatcher,
        bool sslRequired,
        nx::network::NatTraversalSupport natTraversalSupport)
	:
		base_type(sslRequired, natTraversalSupport),
		m_authenticationManager(authenticationManager),
		m_httpMessageDispatcher(httpMessageDispatcher),
        m_persistentConnectionEnabled(true)
	{
	}

    void setPersistentConnectionEnabled(bool value)
    {
        m_persistentConnectionEnabled = value;
    }

protected:
    virtual std::shared_ptr<HttpServerConnection> createConnection(
        std::unique_ptr<AbstractStreamSocket> _socket) override
	{
		auto result = std::make_shared<HttpServerConnection>(
			this,
			std::move(_socket),
			m_authenticationManager,
			m_httpMessageDispatcher);
        result->setPersistentConnectionEnabled(m_persistentConnectionEnabled);
        return result;
	}

private:
    nx_http::server::AbstractAuthenticationManager* const m_authenticationManager;
    nx_http::MessageDispatcher* const m_httpMessageDispatcher;
    bool m_persistentConnectionEnabled;
};
}

#endif  //HTTP_STREAM_SOCKET_SERVER_H
