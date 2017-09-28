#include "get_cloud_modules_xml.h"

#include <nx/network/http/buffer_source.h>

#include <nx/cloud/cdb/client/cdb_request_path.h>

namespace nx {
namespace cdb {
namespace http_handler {

const QString GetCloudModulesXml::kHandlerPath = QLatin1String(kDeprecatedCloudModuleXmlPath);

GetCloudModulesXml::GetCloudModulesXml(
    GenerateModulesXmlFunc generateModulesXmlFunc)
    :
    m_generateModulesXmlFunc(std::move(generateModulesXmlFunc))
{
}

void GetCloudModulesXml::processRequest(
    nx_http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx_http::Request request,
    nx_http::Response* const /*response*/,
    nx_http::RequestProcessedHandler completionHandler)
{
    const auto host = nx_http::getHeaderValue(request.headers, "Host");
    // TODO: #ak in case if host is empty should use public IP address.

    // Note: Host header has format host[:port].
    auto msgBody = std::make_unique<nx_http::BufferSource>(
        "text/xml",
        m_generateModulesXmlFunc(SocketAddress(host).address.toString().toUtf8()));

    completionHandler(nx_http::RequestResult(nx_http::StatusCode::ok, std::move(msgBody)));
}

} // namespace http_handler
} // namespace cdb
} // namespace nx
