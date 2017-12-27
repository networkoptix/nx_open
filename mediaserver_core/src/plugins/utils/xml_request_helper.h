#pragma once

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include <nx/network/http/http_client.h>

namespace nx {
namespace plugins {
namespace utils {

std::unique_ptr<nx_http::HttpClient> makeHttpClient(const QAuthenticator& authenticator);
bool isResponseOK(const nx_http::HttpClient* client);

class XmlRequestHelper
{
public:
    XmlRequestHelper(
        nx::utils::Url url,
        const QAuthenticator& authenticator,
        nx_http::AuthType authType = nx_http::AuthType::authDigest);

    boost::optional<QDomDocument> get(const QString& path);
    bool put(const QString& path, const QString& data = {});
    bool post(const QString& path, const QString& data = {});

    boost::optional<QByteArray> readRawBody();
    boost::optional<QDomDocument> readBody();

protected:
    const nx::utils::Url m_url;
    const std::unique_ptr<nx_http::HttpClient> m_client;
};

std::vector<QDomElement> xmlElements(const QDomNodeList& nodeList);

} // namespace utils
} // namespace plugins
} // namespace nx