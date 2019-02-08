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
    client->setResponseReadTimeout(kRequestTimeout);
    client->setSendTimeout(kRequestTimeout);
    client->setMessageBodyReadTimeout(kRequestTimeout);
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
}

XmlRequestHelper::Result::Result(
    const XmlRequestHelper* parent, QDomElement element, QStringList path)
:
    m_parent(parent),
    m_element(std::move(element)),
    m_path(std::move(path))
{
}

QString XmlRequestHelper::Result::path() const
{
    return containerString(m_path,
        /*delimiter*/ ">.<", /*prefix*/ "<", /*suffix*/ ">", /*empty*/ "root");
}

static QStringList appendPath(QStringList list, QString path)
{
    list.append(std::move(path));
    return list;
}

std::optional<XmlRequestHelper::Result> XmlRequestHelper::Result::child(const QString& name) const
{
    const auto element = m_element.firstChildElement(name);
    if (!element.isNull())
        return Result(m_parent, element, appendPath(m_path, name));

    NX_VERBOSE(m_parent, "Unable to get <%1> from %2", name, path());
    return std::nullopt;
}

std::vector<XmlRequestHelper::Result> XmlRequestHelper::Result::children(const QString& name) const
{
    const auto nodes = m_element.elementsByTagName(name);
    std::vector<Result> results;
    for (int index = 0; index < nodes.size(); ++index)
        results.emplace_back(m_parent, nodes.at(index).toElement(), appendPath(m_path, name));

    return results;
}

std::optional<QString> XmlRequestHelper::Result::string(const QString& name) const
{
    const auto element = child(name);
    if (!element)
        return std::nullopt;

    return element->m_element.text();
}

std::optional<int> XmlRequestHelper::Result::integer(const QString& name) const
{
    const auto value = string(name);
    if (!value)
        return std::nullopt;

    bool isOk = false;
    const auto parsed = value->toInt(&isOk);
    if (isOk)
        return parsed;

    NX_VERBOSE(m_parent, "Value of <%1> from %2 is not an integer: %2", name, path(), *value);
    return std::nullopt;
}

std::optional<bool> XmlRequestHelper::Result::boolean(const QString& name) const
{
    const auto value = string(name);
    if (!value)
        return std::nullopt;

    const auto lowerValue = value->toLower();
    if (lowerValue == lit("true") || lowerValue == lit("on") || lowerValue == lit("1"))
        return true;

    if (lowerValue == lit("false") || lowerValue == lit("off") || lowerValue == lit("0"))
        return false;

    NX_VERBOSE(m_parent, "Value of <%1> from %3is not an boolean: %2", name, path(), *value);
    return std::nullopt;
}

std::optional<QString> XmlRequestHelper::Result::attribute(const QString& name) const
{
    if (!m_element.hasAttribute(name))
        return std::nullopt;

    return m_element.attribute(name);
}

QString XmlRequestHelper::Result::stringOrEmpty(const QString& name) const
{
    const auto value = string(name);
    return value ? *value : QString();
}

int XmlRequestHelper::Result::integerOrZero(const QString& name) const
{
    const auto value = integer(name);
    return value ? *value : 0;
}

bool XmlRequestHelper::Result::booleanOrFalse(const QString& name) const
{
    const auto value = boolean(name);
    return value ? *value : false;
}

QString XmlRequestHelper::Result::attributeOrEmpty(const QString& name) const
{
    return m_element.attribute(name);
}

std::optional<XmlRequestHelper::Result> XmlRequestHelper::get(const QString& path)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doGet(url) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, "Failed to send GET request");
        return std::nullopt;
    }

    return readBody();
}

bool XmlRequestHelper::put(const QString& path, const QString& data)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doPut(url, "application/xml", data.toUtf8()) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, "Failed to send PUT request");
        return false;
    }

    return true;
}

bool XmlRequestHelper::post(const QString& path, const QString& data)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doPost(url, "application/xml", data.toUtf8()) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, "Failed to send POST request");
        return false;
    }

    return true;
}

bool XmlRequestHelper::delete_(const QString& path)
{
    const auto url = nx::network::url::Builder(m_url).setPath(lit("/") + path);
    if (!m_client->doDelete(url) || !isResponseOK(m_client.get()))
    {
        NX_VERBOSE(this, "Failed to send DELETE request");
        return false;
    }

    return true;
}

std::optional<QByteArray> XmlRequestHelper::readRawBody()
{
    const auto response = m_client->fetchEntireMessageBody();
    if (!response)
    {
        NX_DEBUG(this, "Unable to read response: %1",
            SystemError::toString(m_client->lastSysErrorCode()));
        return std::nullopt;
    }

    return *response;
}

std::optional<XmlRequestHelper::Result> XmlRequestHelper::readBody()
{
    const auto response = readRawBody();
    if (!response)
        return std::nullopt;

    QDomDocument document;
    QString parsingError;
    if (!document.setContent(*response, &parsingError))
    {
        NX_WARNING(this, "Unable to parse response: %1", parsingError);
        return std::nullopt;
    }

    const auto element = document.documentElement();
    return Result(this, element, {element.tagName()});
}

QString XmlRequestHelper::idForToStringFromPtr() const
{
    return m_client->url().toString();
}

} // namespace utils
} // namespace plugins
} // namespace nx
