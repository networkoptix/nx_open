// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "private_http_server.h"

#include <nx/network/http/buffer_source.h>
#include <nx/prometheus/registry.h>

#include "http_server_builder.h"

namespace nx::network::http::server {

namespace {

// Prometheus text exposition format content type.
constexpr char kMetricsContentType[] = "text/plain; version=0.0.4";

} // namespace

PrivateHttpServer::PrivateHttpServer(
    nx::prometheus::Registry* registry,
    const SocketAddress& endpoint):
    m_registry(registry)
{
    m_dispatcher.registerRequestProcessorFunc(
        Method::get,
        kMetricsPath,
        [this](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
        {
            RequestResult result(StatusCode::ok);
            result.body =
                std::make_unique<BufferSource>(kMetricsContentType, m_registry->serialize());
            completionHandler(std::move(result));
        });

    Settings settings;
    settings.endpoints.push_back(endpoint);

    m_server = Builder::buildOrThrow(settings, &m_dispatcher);
}

bool PrivateHttpServer::listen()
{
    return m_server->listen();
}

void PrivateHttpServer::pleaseStopSync()
{
    m_server->pleaseStopSync();
}

std::vector<SocketAddress> PrivateHttpServer::endpoints() const
{
    return m_server->endpoints();
}

} // namespace nx::network::http::server
