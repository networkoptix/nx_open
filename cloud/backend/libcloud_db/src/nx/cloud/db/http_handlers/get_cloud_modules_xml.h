#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/move_only_func.h>

namespace nx::cloud::db {

class AuthorizationManager;

namespace http_handler {

class GetCloudModulesXml:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    using GenerateModulesXmlFunc =
        nx::utils::MoveOnlyFunc<QByteArray(const nx::String& /*httpHostHeader*/)>;

    static const QString kHandlerPath;

    GetCloudModulesXml(GenerateModulesXmlFunc generateModulesXmlFunc);

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    GenerateModulesXmlFunc m_generateModulesXmlFunc;
};

} // namespace http_handler
} // namespace nx::cloud::db
