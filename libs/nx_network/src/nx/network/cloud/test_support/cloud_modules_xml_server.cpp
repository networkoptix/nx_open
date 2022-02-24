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

void CloudModulesXmlServer::setModule(const char* resName, const nx::utils::Url& resValue)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_modules[resName] = resValue;
}

QByteArray CloudModulesXmlServer::serializeModules() const
{
    static constexpr char kSequence[] = "<sequence>%1</sequence>";
    static constexpr char kSet[] = "\r\n\t<set resName=\"%1\" resValue=\"%2\"/>";

    NX_MUTEX_LOCKER lock(&m_mutex);

    QByteArray sets;
    for (const auto& [resName, resValue]: m_modules)
        sets.append(nx::format(kSet).args(resName, resValue).toUtf8());
    if (!sets.isEmpty())
        sets.append("\r\n");

    return nx::format(kSequence).arg(sets).toUtf8();
}

void CloudModulesXmlServer::serve(
    network::http::RequestContext /*requestContext*/,
    network::http::RequestProcessedHandler handler)
{
    network::http::RequestResult result(network::http::StatusCode::ok);
    result.dataSource =
        std::make_unique<network::http::BufferSource>(
            "application/xml",
            serializeModules());

    handler(std::move(result));
}

} // namespace nx::network::cloud::test
