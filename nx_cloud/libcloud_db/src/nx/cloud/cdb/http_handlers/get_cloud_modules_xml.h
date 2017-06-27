#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx {
namespace cdb {

class AuthorizationManager;
class CloudModuleUrlProvider;

namespace http_handler {

class GetCloudModulesXml:
    public nx_http::AbstractHttpRequestHandler
{
public:
    static const QString kHandlerPath;

    GetCloudModulesXml(const CloudModuleUrlProvider& cloudModuleUrlProvider);

protected:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

private:
    const CloudModuleUrlProvider& m_cloudModuleUrlProvider;
};

} // namespace http_handler
} // namespace cdb
} // namespace nx
