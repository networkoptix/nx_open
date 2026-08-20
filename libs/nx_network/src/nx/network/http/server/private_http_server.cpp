// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "private_http_server.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/maintenance/get_debug_counters.h>
#include <nx/network/maintenance/get_malloc_info.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/prometheus/registry.h>
#include <nx/reflect/json.h>

#include "http_server_builder.h"

namespace nx::network::http::server {

namespace {

// Prometheus text exposition format content type.
constexpr char kMetricsContentType[] = "text/plain; version=0.0.4";

RequestResult toRequestResult(const HealthMonitor::Status& status)
{
    HealthReply reply{
        .status = status.ok ? HealthReply::Status::ok : HealthReply::Status::unavailable};
    if (!status.failed.empty())
        reply.failed = status.failed;

    RequestResult result(status.ok ? StatusCode::ok : StatusCode::serviceUnavailable);
    result.body = std::make_unique<BufferSource>(
        "application/json",
        nx::reflect::json::serialize(reply));

    return result;
}

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

    m_dispatcher.registerRequestProcessorFunc(
        Method::get,
        kLivenessPath,
        [this](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
        {
            completionHandler(toRequestResult(m_healthMonitor.liveness()));
        });

    m_dispatcher.registerRequestProcessorFunc(
        Method::get,
        kReadinessPath,
        [this](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
        {
            completionHandler(toRequestResult(m_healthMonitor.readiness()));
        });

    m_dispatcher.registerRequestProcessor<maintenance::GetMallocInfo>(
        maintenance::kMallocInfo,
        Method::get);

    m_dispatcher.registerRequestProcessor<maintenance::GetDebugCounters>(
        maintenance::kDebugCounters,
        Method::get);

    Settings settings;
    settings.endpoints.push_back(endpoint);

    m_server = Builder::buildOrThrow(settings, &m_dispatcher);
}

HealthMonitor& PrivateHttpServer::healthMonitor()
{
    return m_healthMonitor;
}

bool PrivateHttpServer::listen()
{
    if (!m_server->listen())
        return false;

    m_healthMonitor.start();
    return true;
}

void PrivateHttpServer::pleaseStopSync()
{
    m_healthMonitor.stop();
    m_server->pleaseStopSync();
}

std::vector<SocketAddress> PrivateHttpServer::endpoints() const
{
    return m_server->endpoints();
}

} // namespace nx::network::http::server
