#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::clusterdb::engine {

class HttpServer
{
public:
    HttpServer(const QnUuid& peerId);

    void registerHandlers(
        const std::string& pathPrefix,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

private:
    void processInfoRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    const QnUuid m_peerId;
};

} // namespace nx::clusterdb::engine
