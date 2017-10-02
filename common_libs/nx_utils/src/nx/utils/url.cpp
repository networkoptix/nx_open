#include <QtNetwork/QHostAddress>
#include <QtCore/QRegExp>
#include <cctype>

#include <nx/utils/log/assert.h>
#include "url.h"

namespace nx {
namespace utils {

namespace detail {

class UrlWithIpV6AndScopeId
{
public:
    UrlWithIpV6AndScopeId(const QString& url): m_url(url.toLower())
    {
        parse();
    }

    QString urlWithoutScopeId() const
    {
        return m_url;
    }

    int scopeId() const
    {
        return m_scopeId;
    }

    bool valid() const
    {
        return !m_url.isEmpty();
    }

private:
    struct Extracted
    {
        QString data;
        int pos;
        int len;
    };

    const static QRegExp kUrlWithIpInBracketsWithScopeIdRx;
    const static QRegExp kUrlIpWithoutBracketsWithScopeIdRx;

    mutable QString m_url;
    mutable int m_scopeId = -1;

    void parse() const
    {
        if (!parseWith(kUrlWithIpInBracketsWithScopeIdRx))
            parseWith(kUrlIpWithoutBracketsWithScopeIdRx);
    }

    bool parseWith(const QRegExp& rx) const
    {
        Extracted scopeId;
        Extracted ip;
        Extracted ipWithScopeId;

        if (!extract(rx, &ipWithScopeId) || !extract(rx, &ip) || !extract(rx, &scopeId))
            return false;

        if (!validateCharAfterIp(ipWithScopeId))
            return false;

        m_url.replace(ipWithScopeId.data, ip.data);
        m_scopeId = scopeId.data.toInt();
    }

    bool extract(const QRegExp& rx, Extracted* target) const
    {
        target->pos = rx.indexIn(m_url);
        if (target->pos == -1)
            return false;

        target->len = rx.matchedLength();
        target->data = rx.cap(1);

        return true;
    }

    bool validateCharAfterIp(const Extracted& extracted) const
    {
        NX_ASSERT(extracted.pos + extracted.len <= m_url.size());
        if (extracted.pos + extracted.len == m_url.size())
            return true;

        const auto nextChar = m_url[extracted.pos + extracted.len];
        if (nextChar != '/' && nextChar != '#' && nextChar != '?')
            return false;

        return true;
    }
};

const QRegExp UrlWithIpV6AndScopeId::kUrlWithIpInBracketsWithScopeIdRx(
    "[a-z][a-z,\\-+.]+:\\/\\/(\\[[0-9:a-f]+\\])%([0-9]+).*");

const QRegExp UrlWithIpV6AndScopeId::kUrlIpWithoutBracketsWithScopeIdRx(
    "[a-z][a-z,\\-+.]+:\\/\\/([0-9:a-f]+)%([0-9]+).*");

} // namespace detail


Url::Url() {}

Url::Url(const QString& url)
{
    urlFromString(url);
}

Url& Url::operator=(const QString& url)
{
    urlFromString(url);
}

void Url::parse(const QString& url)
{
    QUrl result = QUrl::fromUserInput(url);
    if (result.isValid())
         result;

    result = QUrl();
    detail::UrlWithIpV6AndScopeId urlWithIpId(url);
    urlWithIpId
}

void Url::setUrl(const QString& url, ParsingMode mode = TolerantMode);
QString url(FormattingOptions options = FormattingOptions(PrettyDecoded)) const;
QString toString(FormattingOptions options = FormattingOptions(PrettyDecoded)) const;
QString toDisplayString(FormattingOptions options = FormattingOptions(PrettyDecoded)) const;
Q_REQUIRED_RESULT QUrl adjusted(FormattingOptions options) const;

QByteArray toEncoded(FormattingOptions options = FullyEncoded) const;
static Url fromEncoded(const QByteArray &url, ParsingMode mode = TolerantMode);
static Url fromUserInput(const QString &userInput);
static Url fromUserInput(
    const QString &userInput, 
    const QString &workingDirectory, 
    UserInputResolutionOptions options = DefaultResolution);

bool isValid() const;
QString errorString() const;

bool isEmpty() const;
void clear();

void setScheme(const QString &scheme);
QString scheme() const;

void setAuthority(const QString &authority, ParsingMode mode = TolerantMode);
QString authority(ComponentFormattingOptions options = PrettyDecoded) const;

void setUserInfo(const QString &userInfo, ParsingMode mode = TolerantMode);
QString userInfo(ComponentFormattingOptions options = PrettyDecoded) const;

void setUserName(const QString &userName, ParsingMode mode = DecodedMode);
QString userName(ComponentFormattingOptions options = FullyDecoded) const;

void setPassword(const QString &password, ParsingMode mode = DecodedMode);
QString password(ComponentFormattingOptions = FullyDecoded) const;

void setHost(const QString &host, ParsingMode mode = DecodedMode);
QString host(ComponentFormattingOptions = FullyDecoded) const;

void setPort(int port);
int port(int defaultPort = -1) const;

void setPath(const QString &path, ParsingMode mode = DecodedMode);
QString path(ComponentFormattingOptions options = FullyDecoded) const;
QString fileName(ComponentFormattingOptions options = FullyDecoded) const;

bool hasQuery() const;
void setQuery(const QString &query, ParsingMode mode = TolerantMode);
void setQuery(const QUrlQuery &query);
QString query(ComponentFormattingOptions = PrettyDecoded) const;

bool hasFragment() const;
QString fragment(ComponentFormattingOptions options = PrettyDecoded) const;
void setFragment(const QString &fragment, ParsingMode mode = TolerantMode);

Q_REQUIRED_RESULT QUrl resolved(const QUrl &relative) const;

bool isRelative() const;
bool isParentOf(const QUrl &url) const;

bool isLocalFile() const;
static QUrl fromLocalFile(const QString &localfile);
QString toLocalFile() const;

void detach();
bool isDetached() const;

bool operator <(const QUrl &url) const;
bool operator ==(const QUrl &url) const;
bool operator !=(const QUrl &url) const;

bool matches(const QUrl &url, FormattingOptions options) const;

static QString fromPercentEncoding(const QByteArray &);
static QByteArray toPercentEncoding(const QString &,
                                    const QByteArray &exclude = QByteArray(),
                                    const QByteArray &include = QByteArray());
#if defined(Q_OS_DARWIN) || defined(Q_QDOC)
static QUrl fromCFURL(CFURLRef url);
CFURLRef toCFURL() const Q_DECL_CF_RETURNS_RETAINED;
static QUrl fromNSURL(const NSURL *url);
NSURL *toNSURL() const Q_DECL_NS_RETURNS_AUTORELEASED;
#endif

namespace url {

bool equal(const QUrl& lhs, const QUrl& rhs, ComparisonFlags flags)
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

} // namespace url
} // namespace utils
} // namespace nx
