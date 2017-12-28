#include "xml_request_helper.h"

#include <nx/utils/log/log.h>
#include <nx/network/url/url_builder.h>

static const std::chrono::milliseconds kRequestTimeout(4000);

namespace nx {
namespace plugins {
namespace utils {

std::unique_ptr<nx::network::http::HttpClient> makeHttpClient(const QAuthenticator& authenticator)
{
    std::unique_ptr<nx::network::http::HttpClient> client(new nx::network::http::HttpClient);
    client->setResponseReadTimeoutMs((unsigned int)kRequestTimeout.count());
    client->setSendTimeoutMs((unsigned int)kRequestTimeout.count());
    client->setMessageBodyReadTimeoutMs((unsigned int)kRequestTimeout.count());
    client->setUserName(authenticator.user());
    client->setUserPassword(authenticator.password());
    return client;
}

bool isResponseOK(const nx::network::http::HttpClient* client)
{
    if (!client->response())
        return false;
    return client->response()->statusLine.statusCode == nx::network::http::StatusCode::ok;
}


XmlRequestHelper::XmlRequestHelper(
    nx::utils::Url url,
    const QAuthenticator& authenticator,
    nx::network::http::AuthType authType)
:
    m_url(std::move(url)),
    m_client(makeHttpClient(authenticator))
{
    m_client->setAuthType(authType);
    m_client->setIgnoreMutexAnalyzer(true);
}

boost::optional<QDomDocument> XmlRequestHelper::get(const QString& path)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doGet(url) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, lm("Failed to send GET request %1").arg(m_client->url()));
        return boost::none;
    }

    return readBody();
}

bool XmlRequestHelper::put(const QString& path, const QString& data)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doPut(url, "application/xml", data.toUtf8()) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, lm("Failed to send PUT request to %1").arg(m_client->url()));
        return false;
    }

    return true;
}

bool XmlRequestHelper::post(const QString& path, const QString& data)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doPost(url, "application/xml", data.toUtf8()) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, lm("Failed to send POST request to %1").arg(m_client->url()));
        return false;
    }

    return true;
}

boost::optional<QByteArray> XmlRequestHelper::readRawBody()
{
    const auto response = m_client->fetchEntireMessageBody();
    if (!response)
    {
        NX_DEBUG(this, lm("Unable to read response from %1: %2").args(
            m_client->url(), SystemError::toString(m_client->lastSysErrorCode())));
        return boost::none;
    }

    return *response;
}

boost::optional<QDomDocument> XmlRequestHelper::readBody()
{
    const auto response = readRawBody();
    if (!response)
        return boost::none;

    QDomDocument document;
    QString parsingError;
    if (!document.setContent(*response, &parsingError))
    {
        NX_WARNING(this, lm("Unable to parse response from %1: %2").args(
            m_client->url(), parsingError));
        return boost::none;
    }

    return document;
}

} // namespace utils
} // namespace plugins
} // namespace nx