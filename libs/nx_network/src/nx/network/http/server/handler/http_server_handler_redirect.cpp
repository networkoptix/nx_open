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
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    requestContext.response->headers.emplace(
        "Location",
        m_actualLocation.toString().toUtf8());
    completionHandler(StatusCode::movedPermanently);
}

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
