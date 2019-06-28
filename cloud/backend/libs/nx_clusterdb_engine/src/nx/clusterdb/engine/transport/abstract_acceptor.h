#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::clusterdb::engine::transport {

class AbstractAcceptor
{
public:
    virtual ~AbstractAcceptor() = default;

    virtual void registerHandlers(
        const std::string& rootPath,
        nx::network::http::server::rest::MessageDispatcher* messageDispatcher) = 0;
};

} // namespace nx::clusterdb::engine::transport
