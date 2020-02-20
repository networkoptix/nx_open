#include "cloud_modules_xml_server.h"
#include <nx/network/url/url_parse_helper.h>

namespace nx::network::cloud::test {

void CloudModulesXmlServer::setModules(Modules modules)
{
    m_modules = std::move(modules);
}

void CloudModulesXmlServer::registerHandler(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        network::http::Method::get,
        network::url::joinPath(basePath, kRequestPath),
        [this](auto&&... args){ serve(std::forward<decltype(args)>(args)...); });
}

void CloudModulesXmlServer::serve(
    network::http::RequestContext /*requestContext*/,
    network::http::RequestProcessedHandler handler)
{
    network::http::RequestResult result(network::http::StatusCode::ok);
    result.dataSource = std::make_unique<network::http::BufferSource>(
        "application/xml",
        toXml(m_modules));

    handler(std::move(result));
}

QByteArray CloudModulesXmlServer::toXml(const Modules& modules)
{
    static constexpr char kSequence[]= "<sequence>%1</sequence>";
    static constexpr char kSet[] = "\r\n\t<set resName=\"%1\" resValue=\"%2\"/>";

    QByteArray sets;
    for(const auto& [resName, resValue]: modules.m_modules)
        sets.append(lm(kSet).args(resName, resValue));
    if (!sets.isEmpty())
        sets.append("\r\n");

    return lm(kSequence).arg(sets).toUtf8();
}

} // namespace nx::network::cloud::test