// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "url.h"

#include <cctype>

#include <QtCore/QJsonValue>
#include <QtCore/QStringView>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QHostAddress>

#include <nx/utils/log/log.h>
#include <nx/utils/nx_utils_ini.h>

namespace nx::utils {

Url::Url() {}

Url::Url(const QString& url):
    m_url(url)
{
}

Url& Url::operator=(const QString& url)
{
    m_url = url;
    return *this;
}

Url::Url(const std::string_view& url):
    Url(QString::fromUtf8(url.data(), url.size()))
{
}

Url& Url::operator=(const std::string_view& url)
{
    return (*this = QString::fromUtf8(url.data(), url.size()));
}

Url::Url(const std::string& url):
    Url((std::string_view) url)
{
}

Url& Url::operator=(const std::string& url)
{
    return *this = ((std::string_view) url);
}

Url::Url(const char* url):
    Url(QString::fromUtf8(url))
{
}

Url& Url::operator=(const char* url)
{
    return (*this = QString::fromUtf8(url));
}

Url::Url(const QByteArray& url):
    Url(QString::fromUtf8(url))
{
}

Url& Url::operator=(const QByteArray& url)
{
    return (*this = QString::fromUtf8(url));
}

Url::Url(const QUrl& other): m_url(other)
{
}

void Url::setUrl(const QString& url, QUrl::ParsingMode mode)
{
    m_url.setUrl(url, mode);
}

QString Url::url(QUrl::FormattingOptions options) const
{
    if (!m_url.isValid() || m_url.isEmpty())
        return QString();

    return m_url.url(options);
}

QString Url::toString(QUrl::FormattingOptions options) const
{
    return url(options);
}

QString Url::toWebClientStandardViolatingUrl(QUrl::FormattingOptions options) const
{
    nx::utils::Url newUrl(*this);

    if (hasFragment())
    {
        newUrl.setPath(path() + "#" + fragment());
        newUrl.setFragment(QString());
    }

    return newUrl.toString(options).replace("%23", "#");
}

std::string Url::toStdString(QUrl::FormattingOptions options) const
{
    return toString(options).toStdString();
}

Url Url::fromStdString(const std::string& string)
{
    return Url(QString::fromStdString(string));
}

QString Url::toDisplayString(QUrl::FormattingOptions options) const
{
    // Always removes password from URL like it does QUrl.
    return url(options | QUrl::RemovePassword);
}

QByteArray Url::toEncoded(QUrl::FormattingOptions options) const
{
    return m_url.toEncoded(options);
}

QUrl Url::toQUrl() const
{
    return m_url;
}

Url Url::fromQUrl(const QUrl& url)
{
    return Url(url);
}

Url Url::fromUserInput(const QString& userInput)
{
    return Url(QUrl::fromUserInput(userInput));
}

Url Url::fromUserInput(
    const QString &userInput,
    const QString &workingDirectory,
    QUrl::UserInputResolutionOptions options)
{
    return Url(QUrl::fromUserInput(userInput, workingDirectory, options));
}

bool Url::isValid() const
{
    return m_url.isValid();
}

bool Url::isValidHost(const std::string_view& host)
{
    if (host.empty())
        return false;

    Url url;
    url.setHost(QString::fromUtf8(host.data(), host.size()));
    return url.isValid();
}

QString Url::errorString() const
{
    return m_url.errorString();
}

bool Url::isEmpty() const
{
    return m_url.isEmpty();
}

void Url::clear()
{
    m_url.clear();
}

void Url::setScheme(const std::string_view& scheme)
{
    m_url.setScheme(QString::fromUtf8(scheme.data(), scheme.size()));
}

QString Url::scheme() const
{
    return m_url.scheme();
}

void Url::setAuthority(const QString &authority, QUrl::ParsingMode mode)
{
    m_url.setAuthority(authority, mode);
}

QString Url::authority(QUrl::ComponentFormattingOptions options) const
{
    return m_url.authority(options);
}

void Url::setUserInfo(const QString &userInfo, QUrl::ParsingMode mode)
{
    // NOTE: Make sure the QString is null, if it is empty.
    m_url.setUserInfo(userInfo.isEmpty() ? QString() : userInfo, mode);
}

QString Url::userInfo(QUrl::ComponentFormattingOptions options) const
{
    return m_url.userInfo(options);
}

void Url::setUserName(const QString& userName, QUrl::ParsingMode mode)
{
    // NOTE: Make sure the QString is null, if it is empty.
    m_url.setUserName(userName.isEmpty() ? QString() : userName, mode);
}

void Url::setUserName(const std::string_view& userName, QUrl::ParsingMode mode)
{
    setUserName(QString::fromUtf8(userName.data(), (int) userName.size()), mode);
}

void Url::setUserName(const char* userName, QUrl::ParsingMode mode)
{
    setUserName(QString::fromUtf8(userName), mode);
}

QString Url::userName(QUrl::ComponentFormattingOptions options) const
{
    return m_url.userName(options);
}

void Url::setPassword(const QString &password, QUrl::ParsingMode mode)
{
    // NOTE: Make sure the QString is null, if it is empty.
    m_url.setPassword(password.isEmpty() ? QString() : password, mode);
}

void Url::setPassword(const std::string_view& password, QUrl::ParsingMode mode)
{
    setPassword(QString::fromUtf8(password.data(), (int) password.size()), mode);
}

void Url::setPassword(const char* password, QUrl::ParsingMode mode)
{
    setPassword(QString::fromUtf8(password), mode);
}

QString Url::password(QUrl::ComponentFormattingOptions options) const
{
    return m_url.password(options);
}

void Url::setHost(const QString& host, QUrl::ParsingMode mode)
{
    m_url.setHost(host, mode);
}

void Url::setHost(const std::string& host, QUrl::ParsingMode mode)
{
    setHost(QString::fromStdString(host), mode);
}

void Url::setHost(const char* host, QUrl::ParsingMode mode)
{
    setHost(QString(host), mode);
}

QString Url::host(QUrl::ComponentFormattingOptions options) const
{
    return m_url.host(options);
}

void Url::setPort(int port)
{
    m_url.setPort(port);
}

int Url::port(int defaultPort) const
{
    return m_url.port(defaultPort);
}

QString Url::displayAddress(QUrl::ComponentFormattingOptions) const
{
    auto result = host();
    const auto port = m_url.port();
    if (port > 0)
    {
        result += ':';
        result += QString::number(port);
    }
    return result;
}

void Url::setPath(const QString& path, QUrl::ParsingMode mode)
{
    m_url.setPath(path, mode);
}

void Url::setPath(const std::string_view& path, QUrl::ParsingMode mode)
{
    setPath(QString::fromUtf8(path.data(), (int) path.size()), mode);
}

void Url::setPath(const char* path, QUrl::ParsingMode mode)
{
    setPath(QString::fromUtf8(path), mode);
}

QString Url::path(QUrl::ComponentFormattingOptions options) const
{
    return m_url.path(options);
}

QString Url::fileName(QUrl::ComponentFormattingOptions options) const
{
    return m_url.fileName(options);
}

bool Url::hasQuery() const
{
    return m_url.hasQuery();
}

void Url::setQuery(const QString &query, QUrl::ParsingMode mode)
{
    m_url.setQuery(query, mode);
}

void Url::setQuery(const QUrlQuery &query)
{
    m_url.setQuery(query);
}

QString Url::query(QUrl::ComponentFormattingOptions options) const
{
    return m_url.query(options);
}

QString Url::queryItem(const QString& key) const
{
    return QUrlQuery(m_url).queryItemValue(key);
}

void Url::setPathAndQuery(const QString& pathAndQuery, QUrl::ParsingMode mode)
{
    const int queryPos = pathAndQuery.indexOf('?');
    if (queryPos == -1)
    {
        setPath(pathAndQuery, mode);
    }
    else
    {
        setPath(pathAndQuery.left(queryPos), mode);
        setQuery(pathAndQuery.mid(queryPos + 1), mode);
    }
}

void Url::setPathAndQuery(std::string_view pathAndQuery, QUrl::ParsingMode mode)
{
    setPathAndQuery(QString::fromUtf8(pathAndQuery.data(), (int) pathAndQuery.size()), mode);
}

QString Url::pathAndQuery(QUrl::ComponentFormattingOptions options) const
{
    return path(options) + "?" + query(options);
}

bool Url::hasFragment() const
{
    return m_url.hasFragment();
}

QString Url::fragment(QUrl::ComponentFormattingOptions options) const
{
    return m_url.fragment(options);
}

void Url::setFragment(const QString &fragment, QUrl::ParsingMode mode)
{
    m_url.setFragment(fragment, mode);
}

bool Url::isRelative() const
{
    return m_url.isRelative();
}

bool Url::isParentOf(const QUrl &url) const
{
    return m_url.isParentOf(url);
}

bool Url::isLocalFile() const
{
    return m_url.isLocalFile();
}

Url Url::fromLocalFile(const QString &localfile)
{
    return Url(QUrl::fromLocalFile(localfile));
}

QString Url::toLocalFile() const
{
    return m_url.toLocalFile();
}

Url Url::cleanUrl() const
{
    nx::utils::Url url;
    url.setScheme(m_url.scheme());
    url.setHost(m_url.host());
    url.setPort(m_url.port());
    return url;
}

bool Url::operator<(const Url &url) const
{
    return m_url < url.m_url;
}

bool Url::operator==(const Url &url) const
{
    return m_url == url.m_url;
}

bool Url::matches(const Url& url, QUrl::FormattingOptions options) const
{
    return m_url.matches(url.m_url, options);
}

QString Url::fromPercentEncoding(const QByteArray& url)
{
    return QUrl::fromPercentEncoding(url);
}

QString Url::fromPercentEncoding(const std::string_view& url)
{
    return fromPercentEncoding(QByteArray::fromRawData(url.data(), url.size()));
}

QString Url::fromPercentEncoding(const char* url)
{
    return fromPercentEncoding(QByteArray::fromRawData(url, strlen(url)));
}

QByteArray Url::toPercentEncoding(
    const QString& url,
    const QByteArray& exclude,
    const QByteArray& include)
{
    return QUrl::toPercentEncoding(url, exclude, include);
}

std::string Url::toPercentEncoding(
    const std::string_view& str,
    const std::string_view& exclude,
    const std::string_view& include)
{
    // TODO: #akolesnikov This function can be optimized to contain 1 allocation instead of at least 4.

    return toPercentEncoding(
        QString::fromUtf8(str.data(), str.size()),
        QByteArray::fromRawData(exclude.data(), exclude.size()),
        QByteArray::fromRawData(include.data(), include.size())).toStdString();
}

namespace url {

bool equal(const nx::utils::Url& lhs, const nx::utils::Url& rhs, ComparisonFlags flags)
{
    if (flags.testFlag(ComparisonFlag::All))
        return lhs == rhs;

    return (!flags.testFlag(ComparisonFlag::Host) || lhs.host() == rhs.host())
        && (!flags.testFlag(ComparisonFlag::Port) || lhs.port() == rhs.port())
        && (!flags.testFlag(ComparisonFlag::Scheme) || lhs.scheme() == rhs.scheme())
        && (!flags.testFlag(ComparisonFlag::UserName) || lhs.userName() == rhs.userName())
        && (!flags.testFlag(ComparisonFlag::Password) || lhs.password() == rhs.password())
        && (!flags.testFlag(ComparisonFlag::Path) || lhs.path() == rhs.path())
        && (!flags.testFlag(ComparisonFlag::Fragment) || lhs.fragment() == rhs.fragment())
        && (!flags.testFlag(ComparisonFlag::Query) || lhs.query() == rhs.query());
}

nx::utils::Url parseUrlFields(const QString &urlStr, QString scheme)
{
    Url result;

    // Checking if it is just a hostname.
    result.setHost(urlStr, QUrl::StrictMode);
    if (result.isValid())
    {
        result.setScheme(std::move(scheme));
        return result;
    }
    else if (urlStr.endsWith('/'))
    {
        result.setHost(urlStr.chopped(1), QUrl::StrictMode);
        if (result.isValid())
        {
            result.setScheme(std::move(scheme));
            return result;
        }
    }

    // Checking if it is just an authority.
    result.setAuthority(urlStr, QUrl::StrictMode);
    if (result.isValid() && !result.host().isEmpty())
    {
        result.setScheme(std::move(scheme));
        return result;
    }

    // All other cases.
    result.clear();
    result.setUrl(urlStr, QUrl::StrictMode);
    if (result.scheme().isEmpty())
        result.setScheme(std::move(scheme));
    return result;
}

QString hidePassword(nx::utils::Url url)
{
    if (nx::utils::log::showPasswords())
        return url.toString();

    if (url.isValid())
    {
        QUrlQuery resultQuery;
        for (const auto& p : QUrlQuery(url.query()).queryItems())
        {
            const auto percentDecoded = Url::fromPercentEncoding(p.second.toUtf8());
            resultQuery.addQueryItem(p.first, hidePassword(percentDecoded));
        }

        url.setQuery(resultQuery);
        return url.toString(QUrl::DecodeReserved | QUrl::RemovePassword);
    }

    return url.toString();
}

bool webPageHostsEqual(const nx::utils::Url& left, const nx::utils::Url& right)
{
    static const QString kPrefix = "www.";
    const QStringView leftHost{left.host()};
    const QStringView rightHost{right.host()};

    return leftHost.sliced(leftHost.startsWith(kPrefix) ? kPrefix.length() : 0)
        == rightHost.sliced(rightHost.startsWith(kPrefix) ? kPrefix.length() : 0);
}

} // namespace url

QString toString(const nx::utils::Url& value)
{
    if (nx::utils::log::showPasswords())
        return value.toString();

    return value.toDisplayString();
}

} // namespace nx::utils

void serialize(QnJsonContext* /*ctx*/, const nx::utils::Url& url, QJsonValue* target)
{
    *target = QJsonValue(url.toString());
}

bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, nx::utils::Url* target)
{
    *target = nx::utils::Url(value.toString());
    return true;
}
