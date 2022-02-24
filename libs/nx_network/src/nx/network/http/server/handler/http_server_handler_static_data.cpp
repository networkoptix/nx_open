// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_handler_static_data.h"

#include <memory>

#include "../../buffer_source.h"

namespace nx::network::http::server::handler {

StaticData::StaticData(const std::string& mimeType, nx::Buffer response):
    m_mimeType(mimeType),
    m_response(std::move(response))
{
}

void StaticData::processRequest(
    RequestContext /*requestContext*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    completionHandler(RequestResult(
        StatusCode::ok,
        std::make_unique<BufferSource>(m_mimeType, m_response)));
}

} // namespace nx::network::http::server::handler
