#include "url_builder.h"

namespace nx::network::url {

Builder::Builder(const nx::utils::Url& url):
    m_url(url)
{
}

nx::utils::Url Builder::toUrl() const
{
    return m_url;
}

Builder::operator utils::Url() const
{
    return toUrl();
}

QString Builder::toString(QUrl::FormattingOptions options) const
{
    return toUrl().toString(options);
}

Builder& Builder::setScheme(const QString& scheme)
{
    m_url.setScheme(scheme);
    return *this;
}

Builder& Builder::setAuthority(const QString& authority, QUrl::ParsingMode mode)
{
    m_url.setAuthority(authority, mode);
    return *this;
}

Builder& Builder::setUserInfo(const QString& userInfo, QUrl::ParsingMode mode)
{
    m_url.setUserInfo(userInfo, mode);
    return *this;
}

Builder& Builder::setUserName(const QString& userName, QUrl::ParsingMode mode)
{
    m_url.setUserName(userName, mode);
    return *this;
}

Builder& Builder::setPassword(const QString& password, QUrl::ParsingMode mode)
{
    m_url.setPassword(password, mode);
    return *this;
}

Builder& Builder::setHost(const QString& host, QUrl::ParsingMode mode)
{
    m_url.setHost(host, mode);
    return *this;
}

Builder& Builder::setPort(int port)
{
    m_url.setPort(port);
    return *this;
}

Builder& Builder::setEndpoint(const SocketAddress& endpoint, QUrl::ParsingMode mode)
{
    m_url.setHost(endpoint.address.toString(), mode);
    if (endpoint.port > 0)
        m_url.setPort(endpoint.port);
    return *this;
}

Builder& Builder::setPath(const QString& path, QUrl::ParsingMode mode)
{
    // QUrl does not like several slashes at the beginning, as well as no slash at all.
    auto actualPath = normalizePath(path);

    if (!actualPath.isEmpty() && !actualPath.startsWith('/'))
        actualPath = '/' + actualPath;

    m_url.setPath(actualPath, mode);
    return *this;
}

Builder& Builder::setPath(const std::string& path, QUrl::ParsingMode mode)
{
    return setPath(QString::fromStdString(path), mode);
}

Builder& Builder::setPath(const char* path, QUrl::ParsingMode mode)
{
    return setPath(QString::fromUtf8(path), mode);
}

Builder& Builder::setPath(const QByteArray& path, QUrl::ParsingMode mode)
{
    return setPath(QString::fromUtf8(path), mode);
}

Builder& Builder::appendPath(const QString& path, QUrl::ParsingMode mode)
{
    auto value = m_url.path();
    if (!value.endsWith('/') && !path.startsWith('/'))
        value += '/';
    value += path;
    setPath(value, mode);
    return *this;
}

Builder& Builder::appendPath(const std::string& path, QUrl::ParsingMode mode)
{
    return appendPath(QString::fromStdString(path), mode);
}

Builder& Builder::appendPath(const char* path, QUrl::ParsingMode mode)
{
    return appendPath(std::string(path), mode);
}

Builder& Builder::appendPath(const QByteArray& path, QUrl::ParsingMode mode)
{
    return appendPath(QString::fromUtf8(path), mode);
}

Builder& Builder::setQuery(const QString& query, QUrl::ParsingMode mode)
{
    m_url.setQuery(query, mode);
    return *this;
}

Builder& Builder::setQuery(const QUrlQuery& query)
{
    m_url.setQuery(query);
    return *this;
}

Builder& Builder::setFragment(const QString& fragment, QUrl::ParsingMode mode)
{
    m_url.setFragment(fragment, mode);
    return *this;
}

}// namespace nx::network::url
