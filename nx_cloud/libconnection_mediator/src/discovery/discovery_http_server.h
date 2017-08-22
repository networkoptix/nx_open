#pragma once

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx {
namespace cloud {
namespace discovery {

class RegisteredPeerPool;

class HttpServer
{
public:
    HttpServer(
        nx_http::server::rest::MessageDispatcher* httpMessageDispatcher,
        RegisteredPeerPool* registeredPeerPool);

private:
    nx_http::server::rest::MessageDispatcher* m_httpMessageDispatcher;
    RegisteredPeerPool* m_registeredPeerPool;
};

} // namespace discovery
} // namespace cloud
} // namespace nx
