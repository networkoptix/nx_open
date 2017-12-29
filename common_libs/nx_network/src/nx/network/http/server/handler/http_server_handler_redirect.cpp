#include "http_server_handler_redirect.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "../../buffer_source.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace handler {

Redirect::Redirect(const nx::utils::Url& actualLocation):
    m_actualLocation(actualLocation)
{
}

void Redirect::processRequest(
    nx::network::http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request /*request*/,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    response->headers.emplace("Location", m_actualLocation.toString().toUtf8());
    completionHandler(nx::network::http::StatusCode::movedPermanently);
}

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
