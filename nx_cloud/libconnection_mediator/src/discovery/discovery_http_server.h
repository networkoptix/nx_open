#pragma once

#include <nx/network/http/server/http_message_dispatcher.h>

namespace nx {
namespace cloud {
namespace discovery {

class RegisteredPeerPool;

class HttpServer
{
public:
    HttpServer(
        nx_http::MessageDispatcher* httpMessageDispatcher,
        RegisteredPeerPool* registeredPeerPool);

private:
    nx_http::MessageDispatcher* m_httpMessageDispatcher;
    RegisteredPeerPool* m_registeredPeerPool;
};

} // namespace nx
} // namespace cloud
} // namespace discovery
