// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::network::maintenance::statistics {

class NX_NETWORK_API Server {
public:
    Server();

    void registerRequestHandlers(
        const std::string& basePath,
        http::server::rest::MessageDispatcher* messageDispatcher);

private:
    void getStatisticsGeneral(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler);

private:
    std::chrono::steady_clock::time_point m_processStartTime;
};

} // namespace nx::network::maintenance::statistics
