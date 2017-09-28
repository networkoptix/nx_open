#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cdb {

class AuthorizationManager;

namespace http_handler {

class GetCloudModulesXml:
    public nx_http::AbstractHttpRequestHandler
{
public:
    using GenerateModulesXmlFunc = 
        nx::utils::MoveOnlyFunc<QByteArray(const nx::String& /*httpHostHeader*/)>;

    static const QString kHandlerPath;

    GetCloudModulesXml(GenerateModulesXmlFunc generateModulesXmlFunc);

protected:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

private:
    GenerateModulesXmlFunc m_generateModulesXmlFunc;
};

} // namespace http_handler
} // namespace cdb
} // namespace nx
