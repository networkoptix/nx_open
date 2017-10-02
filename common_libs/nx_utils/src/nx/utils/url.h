#pragma once

#include <QtCore/QUrl>

namespace nx {
namespace utils {


class Url
{
public:
    Url();
    Url(const QString& url);
    Url& operator=(const QString& url);

    Url(const Url& /*other*/) = default;
    Url& operator=(const Url& /*other*/) = default;    

    Url(Url&& /*other*/) = default;
    Url& operator=(Url&& /*other*/) = default;


    inline void swap(Url& cother) Q_DECL_NOTHROW { m_url.swap(other); }

    void setUrl(const QString& url, QUrl::ParsingMode mode = QUrl::TolerantMode);
    QString url(QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;
    QString toString(QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;
    QString toDisplayString(QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    QByteArray toEncoded(QUrl::FormattingOptions options = QUrl::FullyEncoded) const;
    static Url fromEncoded(const QByteArray &url, QUrl::ParsingMode mode = QUrl::TolerantMode);
    static Url fromUserInput(const QString &userInput);
    static Url fromUserInput(
        const QString &userInput, 
        const QString &workingDirectory, 
        QUrl::UserInputResolutionOptions options = QUrl::DefaultResolution);

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

private:
    QUrl m_url;
    boost::optional<int> m_IpV6ScopeId;

    static QUrl urlFromString(const QString& url);
};


namespace url {

enum class UrlPart
{
    Scheme = 0x01,
    UserName = 0x02,
    Password = 0x04,
    Authority = UserName | Password,
    Host = 0x08,
    Port = 0x10,
    Address = Host | Port,
    Path = 0x20,
    Fragment = 0x40,
    Query = 0x80,
    All = Scheme | Authority | Address | Path | Fragment | Query
};

using ComparisonFlag = UrlPart;
Q_DECLARE_FLAGS(ComparisonFlags, ComparisonFlag)

NX_UTILS_API bool equal(const QUrl& lhs, const QUrl& rhs, ComparisonFlags flags = ComparisonFlag::All);

inline bool addressesEqual(const QUrl& lhs, const QUrl& rhs)
{
    return equal(lhs, rhs, ComparisonFlag::Address);
}

} // namespace url
} // namespace utils
} // namespace nx
