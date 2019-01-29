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
    m_maintenancePath = url::joinPath(basePath, kMaintenance);

    messageDispatcher->registerRequestProcessor<GetMallocInfo>(
        url::joinPath(m_maintenancePath, kMallocInfo).c_str(),
        http::Method::get);

    m_logServer.registerRequestHandlers(url::joinPath(m_maintenancePath, kLog), messageDispatcher);
}

std::string Server::maintenancePath() const
{
    return m_maintenancePath;
}

} // namespace nx::network::maintenance
