// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_modules_xml_server.h"

#include <nx/network/url/url_parse_helper.h>
#include <nx/network/cloud/basic_cloud_module_url_fetcher.h>

namespace nx::network::cloud::test {

void CloudModulesXmlServer::registerRequestHandlers(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    static constexpr char kCloudModulesXml[] = "/cloud_modules.xml";

    m_cloudModulesXmlPath = network::url::joinPath(basePath, kCloudModulesXml);

    messageDispatcher->registerRequestProcessorFunc(
        network::http::Method::get,
        m_cloudModulesXmlPath,
        [this](auto&&... args) { serve(std::forward<decltype(args)>(args)...); });
}

std::string CloudModulesXmlServer::cloudModulesXmlPath() const
{
    return m_cloudModulesXmlPath;
}

void CloudModulesXmlServer::setCdbUrl(const nx::utils::Url& url)
{
    setModule(kCloudDbModuleName, url);
}

void CloudModulesXmlServer::setHpmTcpUrl(const nx::utils::Url& url)
{
    setModule(kConnectionMediatorTcpUrlName, url);
}

void CloudModulesXmlServer::setHpmUdpUrl(const nx::utils::Url& url)
{
    setModule(kConnectionMediatorUdpUrlName, url);
}

void CloudModulesXmlServer::setNotificationModuleUrl(const nx::utils::Url& url)
{
    setModule(kNotificationModuleName, url);
}

void CloudModulesXmlServer::setSpeedTestUrl(const nx::utils::Url& url)
{
    setModule(kSpeedTestModuleName, url);
}

void CloudModulesXmlServer::setModule(const std::string& resName, const nx::utils::Url& resValue)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_modules[resName] = resValue;
}

std::string CloudModulesXmlServer::xml() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    std::string xml = "<sequence>\r\n";
    for (const auto& [resName, resValue]: m_modules)
        xml += "\t<set resName=\"" + resName + "\" resValue=\"" + resValue.toStdString() + "\"/>\r\n";
    xml += "</sequence>\r\n";

    return xml;
}

void CloudModulesXmlServer::clear()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_modules.clear();
}

void CloudModulesXmlServer::serve(
    network::http::RequestContext /*requestContext*/,
    network::http::RequestProcessedHandler handler)
{
    network::http::RequestResult result(network::http::StatusCode::ok);
    result.body = std::make_unique<network::http::BufferSource>("application/xml", xml());

    handler(std::move(result));
}

} // namespace nx::network::cloud::test
