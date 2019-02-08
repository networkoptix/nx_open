#include "get_cloud_modules_xml.h"

#include <nx/network/http/buffer_source.h>

#include <nx/cloud/db/client/cdb_request_path.h>

namespace nx::cloud::db {
namespace http_handler {

const QString GetCloudModulesXml::kHandlerPath = QLatin1String(kDeprecatedCloudModuleXmlPath);

GetCloudModulesXml::GetCloudModulesXml(
    GenerateModulesXmlFunc generateModulesXmlFunc)
    :
    m_generateModulesXmlFunc(std::move(generateModulesXmlFunc))
{
}

void GetCloudModulesXml::processRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace nx::network;

    const auto host = http::getHeaderValue(requestContext.request.headers, "Host");
    // TODO: #ak in case if host is empty should use public IP address.

    // Note: Host header has format host[:port].
    auto msgBody = std::make_unique<http::BufferSource>(
        "text/xml",
        m_generateModulesXmlFunc(network::SocketAddress(host).address.toString().toUtf8()));

    completionHandler(http::RequestResult(http::StatusCode::ok, std::move(msgBody)));
}

} // namespace http_handler
} // namespace nx::cloud::db
