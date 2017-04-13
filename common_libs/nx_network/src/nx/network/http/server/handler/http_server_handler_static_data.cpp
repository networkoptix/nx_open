#include "http_server_handler_static_data.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "../../buffer_source.h"

namespace nx_http {
namespace server {
namespace handler {

StaticData::StaticData(const nx_http::StringType& mimeType, QByteArray response):
    m_mimeType(mimeType),
    m_response(std::move(response))
{
}

void StaticData::processRequest(
    nx_http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx_http::Request /*request*/,
    nx_http::Response* const /*response*/,
    nx_http::RequestProcessedHandler completionHandler)
{
    completionHandler(
        RequestResult(
            nx_http::StatusCode::ok,
            std::make_unique<BufferSource>(m_mimeType, m_response)));
}

} // namespace handler
} // namespace server
} // namespace nx_http
