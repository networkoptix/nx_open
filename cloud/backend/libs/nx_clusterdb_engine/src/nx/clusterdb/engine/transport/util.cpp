#include "util.h"

#include "../http/http_paths.h"

namespace nx::clusterdb::engine::transport {

std::string extractSystemIdFromHttpRequest(
    const nx::network::http::RequestContext& requestContext)
{
    using namespace nx::network::http;

    const auto systemId =
        requestContext.requestPathParams.getByName(kClusterIdParamName);
    if (!systemId.empty())
        return systemId;

    const auto& request = requestContext.request;

    auto authorizationHeaderIter = request.headers.find(header::Authorization::NAME);
    if (authorizationHeaderIter == request.headers.end())
        return std::string();

    header::Authorization authorization;
    if (!authorization.parse(authorizationHeaderIter->second))
        return std::string();

    return authorization.userid().toStdString();
}

} // namespace nx::clusterdb::engine::transport
