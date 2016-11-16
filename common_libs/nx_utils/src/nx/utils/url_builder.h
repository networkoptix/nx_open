#pragma once

#include <QtCore/QUrl>

namespace nx {
namespace utils {

class UrlBuilder
{
public:
    UrlBuilder(const QUrl& url = QUrl()): m_url(url) {}

    QUrl toUrl() const { return m_url; }
    operator QUrl() const { return toUrl(); }

    QString toString(QUrl::FormattingOptions options =
        QUrl::FormattingOptions(QUrl::PrettyDecoded)) const
    {
        return toUrl().toString(options);
    }

    UrlBuilder& setScheme(const QString& scheme)
    {
        m_url.setScheme(scheme);
        return *this;
    }
    UrlBuilder& setAuthority(const QString& authority, QUrl::ParsingMode mode = QUrl::TolerantMode)
    {
        m_url.setAuthority(authority, mode);
        return *this;
    }
    UrlBuilder& setUserInfo(const QString& userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode)
    {
        m_url.setUserInfo(userInfo, mode);
        return *this;
    }
    UrlBuilder& setUserName(const QString& userName, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setUserName(userName, mode);
        return *this;
    }
    UrlBuilder& setPassword(const QString& password, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setPassword(password, mode);
        return *this;
    }
    UrlBuilder& setHost(const QString& host, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setHost(host, mode);
        return *this;
    }
    UrlBuilder& setPort(int port)
    {
        m_url.setPort(port);
        return *this;
    }
    UrlBuilder& setPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setPath(path, mode);
        return *this;
    }
    UrlBuilder& setQuery(const QString& query, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setQuery(query, mode);
        return *this;
    }
    UrlBuilder& setQuery(const QUrlQuery& query)
    {
        m_url.setQuery(query);
        return *this;
    }
    UrlBuilder& setFragment(const QString& fragment, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        m_url.setFragment(fragment, mode);
        return *this;
    }

private:
    QUrl m_url;
};

} // namespace utils
} // namespace nx
