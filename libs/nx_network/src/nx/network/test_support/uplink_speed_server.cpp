// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uplink_speed_test_server.h"

#include "../url/url_parse_helper.h"

namespace nx::network::cloud::speed_test::test {

void UplinkSpeedTestServer::registerRequestHandlers(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    m_speedTestRequestPath = network::url::joinPath(basePath, speed_test::http::kApiPath);
    messageDispatcher->registerRequestProcessorFunc(
        network::http::kAnyMethod,
        m_speedTestRequestPath,
        [this](auto&& ... args) { serve(std::forward<decltype(args)>(args)...); });
}

void UplinkSpeedTestServer::setBandwidthTestInProgressHandler(
    nx::utils::MoveOnlyFunc<void(int /*xTestSequence*/)> handler)
{
    m_bandwidthTestInProgressHandler = std:: move(handler);
}

const std::string& UplinkSpeedTestServer::requestPath() const
{
    return m_speedTestRequestPath;
}

void UplinkSpeedTestServer::serve(
    nx::network::http::RequestContext request,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto it = request.request.headers.find("X-Test-Sequence");
    if (it == request.request.headers.end()) //< Performing a ping test in this case.
        return completionHandler(nx::network::http::StatusCode::ok);

    if (m_bandwidthTestInProgressHandler)
        m_bandwidthTestInProgressHandler(nx::utils::stoi(it->second));

    network::http::RequestResult result(network::http::StatusCode::ok);
    result.headers.emplace(it->first, it->second);

    completionHandler(std::move(result));
}

} // namespace nx::network::cloud::speed_test::test
