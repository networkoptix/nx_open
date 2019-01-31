#include <cctype>

#include <QtNetwork/QHostAddress>
#include <QtCore/QRegExp>

#include <nx/utils/log/assert.h>
#include <nx/utils/nx_utils_ini.h>

#include "url.h"

namespace nx {
namespace utils {

namespace detail {

class UrlWithIpV6AndScopeId
{
public:
    UrlWithIpV6AndScopeId(const QString& url):
        kUrlRx("^[a-z][a-z,\\-+.]+:\\/\\/[^\\]]*(\\[([0-9:a-f]+)%([0-9]+)\\])"),
        m_url(url.toLower())
    {
        parse();
    }

    QString urlWithoutScopeId() const
    {
        return m_newUrl;
    }

    int scopeId() const
    {
        return m_scopeId;
    }

    bool valid() const
    {
        return !m_newUrl.isEmpty();
    }

private:
    const QRegExp kUrlRx;

    const QString m_url;
    mutable QString m_newUrl;
    mutable int m_scopeId = -1;

    bool parse() const
    {
        int pos;
        if ((pos = kUrlRx.indexIn(m_url)) == -1)
            return false;

        QString ipWithScopeId = kUrlRx.cap(1);
        QString ip = kUrlRx.cap(2);
        ip = '[' + ip + ']';

        if (!validateCharAfterIp(kUrlRx.matchedLength()))
            return false;

        m_newUrl = m_url;
        m_newUrl.replace(ipWithScopeId, ip);
        m_scopeId = kUrlRx.cap(3).toInt();

        return true;
    }

    bool validateCharAfterIp(int pos) const
    {
        NX_ASSERT(pos <= m_url.size());
        if (pos == m_url.size())
            return true;

        const auto nextChar = m_url[pos];
        if (nextChar != '/' && nextChar != '#' && nextChar != '?' && nextChar != ':')
            return false;

        return true;
    }
};

class IpWithScopeId
{
public:
    IpWithScopeId(const QString& ip):
        kIpInBracketsWithScopeIdRx("^\\([[0-9:a-f]+\\])%([0-9]+)$"),
        kIpWithoutBracketsWithScopeIdRx("^([0-9:a-f]+)%([0-9]+)$"),
        m_ip(ip)
    {
        if(!parseWith(kIpInBracketsWithScopeIdRx))
            parseWith(kIpWithoutBracketsWithScopeIdRx);
    }

    QString ip() const { return m_newIp; }
    int scopeId() const { return m_scopeId; }
    bool valid() const { return !m_newIp.isEmpty(); }

private:
    const QRegExp kIpInBracketsWithScopeIdRx;
    const QRegExp kIpWithoutBracketsWithScopeIdRx;

    const QString m_ip;
    mutable QString m_newIp;
    mutable int m_scopeId = -1;

    bool parseWith(const QRegExp& rx) const
    {
        if (rx.indexIn(m_ip) == -1)
            return false;

        m_newIp = rx.cap(1);
        m_scopeId = rx.cap(2).toInt();

        return true;
    }
};

} // namespace detail


Url::Url() {}

Url::Url(const QString& url)
{
    parse(
        url,
        [this](const QString& url)
        {
            m_url = QUrl(url);
        });
}

Url& Url::operator=(const QString& url)
{
    parse(
        url,
        [this](const QString& url)
        {
            m_url = QUrl(url);
        });
    return *this;
}

Url::Url(const std::string& url):
    Url(QString::fromStdString(url))
{
}

Url& Url::operator=(const std::string& url)
{
    return (*this = QString::fromStdString(url));
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

template<typename SetUrlFunc>
void Url::parse(const QString& url, SetUrlFunc setUrlFunc)
{
    setUrlFunc(url);
    if (m_url.isValid() && !m_url.isEmpty())
         return;

    detail::UrlWithIpV6AndScopeId urlWithIpId(url);
    if (urlWithIpId.valid())
    {
        setUrlFunc(urlWithIpId.urlWithoutScopeId());
        m_ipV6ScopeId = urlWithIpId.scopeId();
    }
}

void Url::setUrl(const QString& url, QUrl::ParsingMode mode)
{
    parse(
        url,
        [this, mode](const QString& url)
        {
            m_url.setUrl(url, mode);
        });
}

QString Url::url(QUrl::FormattingOptions options) const
{
    if (!m_url.isValid() || m_url.isEmpty())
        return QString();

    QString result = m_url.url(options);
    if ((bool) m_ipV6ScopeId)
        result.replace(m_url.host(), m_url.host() + '%' + QString::number(*m_ipV6ScopeId));

    return result;
}

QString Url::toString(QUrl::FormattingOptions options) const
{
    return url(options);
}

std::string Url::toStdString(QUrl::FormattingOptions options) const
{
    return toString(options).toStdString();
}

QString Url::toDisplayString(QUrl::FormattingOptions options) const
{
    // Always removes password from URL like it does QUrl.
    return url(options | QUrl::RemovePassword);
}

QByteArray Url::toEncoded(QUrl::FormattingOptions options) const
{
    if (!m_url.isValid() || m_url.isEmpty())
        return QByteArray();

    QByteArray result = m_url.toEncoded(options);
    if ((bool) m_ipV6ScopeId)
    {
        result.replace(
            m_url.host().toLatin1(),
            m_url.host().toLatin1() + '%' + QByteArray::number(*m_ipV6ScopeId));
    }

    return result;
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
    m_ipV6ScopeId = boost::none;
    return m_url.clear();
}

void Url::setScheme(const QString &scheme)
{
    m_url.setScheme(scheme);
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

void Url::setUserName(const QString &userName, QUrl::ParsingMode mode)
{
    // NOTE: Make sure the QString is null, if it is empty.
    m_url.setUserName(userName.isEmpty() ? QString() : userName, mode);
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

QString Url::password(QUrl::ComponentFormattingOptions options) const
{
    return m_url.password(options);
}

void Url::setHost(const QString &host, QUrl::ParsingMode mode)
{
    QUrl newUrl;
    newUrl.setHost(host);
    if (newUrl.isValid() && !newUrl.isEmpty())
    {
        m_ipV6ScopeId = boost::none;
        m_url.setHost(host);
        return;
    }

    detail::IpWithScopeId ipWithScopeId(host);
    if (!ipWithScopeId.valid())
        return;

    m_url.setHost(ipWithScopeId.ip(), mode);
    m_ipV6ScopeId = ipWithScopeId.scopeId();
}

QString Url::host(QUrl::ComponentFormattingOptions options) const
{
    return (bool) m_ipV6ScopeId
        ? m_url.host(options) + '%' + QString::number(*m_ipV6ScopeId)
        : m_url.host(options);
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
        result += L':';
        result += QString::number(port);
    }
    return result;
}

void Url::setPath(const QString &path, QUrl::ParsingMode mode)
{
    m_url.setPath(path, mode);
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

bool Url::operator <(const Url &url) const
{
    if (m_url < url.m_url)
        return true;
    else if (url.m_url < m_url)
        return false;

    if ((bool) m_ipV6ScopeId && (bool) url.m_ipV6ScopeId)
        return *m_ipV6ScopeId < *url.m_ipV6ScopeId;

    if (!(bool) m_ipV6ScopeId && !(bool) url.m_ipV6ScopeId)
        return false;

    if ((bool) m_ipV6ScopeId)
        return false;

    return true;
}

bool Url::operator ==(const Url &url) const
{
    return m_url == url.m_url && m_ipV6ScopeId == url.m_ipV6ScopeId;
}

bool Url::operator !=(const Url &url) const
{
    return !operator==(url);
}

bool Url::matches(const Url& url, QUrl::FormattingOptions options) const
{
    return m_url.matches(url.m_url, options);
}

QString Url::fromPercentEncoding(const QByteArray& url)
{
    QString result = QUrl::fromPercentEncoding(url);
    if (!result.isEmpty())
        return result;

    auto urlString = QString::fromUtf8(url);
    detail::UrlWithIpV6AndScopeId urlWithIp(urlString);
    if (!urlWithIp.valid())
        return QString();

    QUrl qurl(urlWithIp.urlWithoutScopeId());
    if (!qurl.isValid() || qurl.isEmpty())
        return QString();

    result = QUrl::fromPercentEncoding(urlWithIp.urlWithoutScopeId().toUtf8());
    result.replace(qurl.host(), qurl.host() + '%' + urlWithIp.scopeId());

    return result;
}

QByteArray Url::toPercentEncoding(
    const QString& url,
    const QByteArray& exclude,
    const QByteArray& include)
{
    QByteArray result = QUrl::toPercentEncoding(url, exclude, include);
    if (!result.isEmpty())
        return result;

    detail::UrlWithIpV6AndScopeId urlWithIp(url);
    if (!urlWithIp.valid())
        return QByteArray();

    QUrl qurl(urlWithIp.urlWithoutScopeId());
    if (!qurl.isValid() || qurl.isEmpty())
        return QByteArray();

    result = QUrl::toPercentEncoding(urlWithIp.urlWithoutScopeId().toUtf8(), exclude, include);
    result.replace(
        qurl.host().toUtf8(),
        qurl.host().toUtf8() + '%' + QByteArray::number(urlWithIp.scopeId()));

    return result;
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

QString hidePassword(const QString& urlStr)
{
    if (displayPasswordInLogs())
        return urlStr;

    nx::utils::Url url(urlStr);
    if (url.isValid())
        url.toDisplayString();

    return urlStr;
}

bool displayPasswordInLogs()
{
    return nx::utils::ini().displayUrlPasswordInLogs;
}

} // namespace url
} // namespace utils
} // namespace nx
