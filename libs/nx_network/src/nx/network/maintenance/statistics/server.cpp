// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_types.h>
#include <nx/fusion/model_functions.h>

#include <nx/network/url/url_parse_helper.h>

#include "request_path.h"
#include "statistics.h"

namespace nx::network::maintenance::statistics {

Server::Server():
    m_processStartTime(std::chrono::steady_clock::now())
{
}

void Server::registerRequestHandlers(
    const std::string& basePath,
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::get,
        url::joinPath(basePath, kGeneral),
        [this](auto&& ... args){ getStatisticsGeneral(std::forward<decltype(args)>(args)...); });
}

void Server::getStatisticsGeneral(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    using namespace std::chrono;
    Statistics stats{
        duration_cast<milliseconds>(steady_clock::now() - m_processStartTime)};

    http::RequestResult result(http::StatusCode::ok);
    result.body = std::make_unique<http::BufferSource>(
        http::header::ContentType::kJson,
        QJson::serialized(std::move(stats)));

    completionHandler(std::move(result));
}


} // namespace nx::network::maintenance::statistics