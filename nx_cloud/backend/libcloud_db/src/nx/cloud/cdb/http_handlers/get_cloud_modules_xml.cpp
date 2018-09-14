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
    nx::network::http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request request,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto host = nx::network::http::getHeaderValue(request.headers, "Host");
    // TODO: #ak in case if host is empty should use public IP address.

    // Note: Host header has format host[:port].
    auto msgBody = std::make_unique<nx::network::http::BufferSource>(
        "text/xml",
        m_generateModulesXmlFunc(network::SocketAddress(host).address.toString().toUtf8()));

    completionHandler(nx::network::http::RequestResult(nx::network::http::StatusCode::ok, std::move(msgBody)));
}

} // namespace http_handler
} // namespace cdb
} // namespace nx
