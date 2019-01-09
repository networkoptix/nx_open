#include "server.h"

#include <nx/network/http/http_types.h>
#include <nx/network/url/url_parse_helper.h>

#include "get_malloc_info.h"
#include "request_path.h"

namespace nx::network::maintenance {

void Server::registerRequestHandlers(
    const std::string& basePath,
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessor<GetMallocInfo>(
        url::joinPath(basePath, kMaintenance, kMallocInfo).c_str(),
        http::Method::get);
}

} // namespace nx::network::maintenance
