#include "http_server_handler_redirect.h"

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "../../buffer_source.h"

namespace nx_http {
namespace server {
namespace handler {

Redirect::Redirect(const nx::utils::Url& actualLocation):
    m_actualLocation(actualLocation)
{
}

void Redirect::processRequest(
    nx_http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx_http::Request /*request*/,
    nx_http::Response* const response,
    nx_http::RequestProcessedHandler completionHandler)
{
    response->headers.emplace("Location", m_actualLocation.toString().toUtf8());
    completionHandler(nx_http::StatusCode::movedPermanently);
}

} // namespace handler
} // namespace server
} // namespace nx_http
