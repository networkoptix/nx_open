#include "http_server_handler_static_data.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "../../buffer_source.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace handler {

StaticData::StaticData(const nx::network::http::StringType& mimeType, QByteArray response):
    m_mimeType(mimeType),
    m_response(std::move(response))
{
}

void StaticData::processRequest(
    nx::network::http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request /*request*/,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    completionHandler(
        RequestResult(
            nx::network::http::StatusCode::ok,
            std::make_unique<BufferSource>(m_mimeType, m_response)));
}

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
