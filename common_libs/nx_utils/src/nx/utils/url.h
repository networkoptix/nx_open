#pragma once

#include <QtCore/QUrl>
#include <QtCore/QJsonValue>
#include <QtXml/QtXml>
#include <boost/optional.hpp>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace utils {


class NX_UTILS_API Url
{
public:
    Url();
    Url(const QString& url);
    Url& operator=(const QString& url);

    Url(const Url& /*other*/) = default;
    Url& operator=(const Url& /*other*/) = default;

    Url(Url&& /*other*/) = default;
    Url& operator=(Url&& /*other*/) = default;


    inline void swap(Url& other) Q_DECL_NOTHROW { m_url.swap(other.m_url); }

    void setUrl(const QString& url, QUrl::ParsingMode mode = QUrl::TolerantMode);
    QString url(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    QString toString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    QString toDisplayString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    QByteArray toEncoded(QUrl::FormattingOptions options = QUrl::FullyEncoded) const;
    QUrl toQUrl() const;

    static Url fromQUrl(const QUrl& url);
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

    void setAuthority(const QString &authority, QUrl::ParsingMode mode = QUrl::TolerantMode);
    QString authority(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

    void setUserInfo(const QString &userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode);
    QString userInfo(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

    void setUserName(const QString &userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    QString userName(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;

    void setPassword(const QString &password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    QString password(QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setHost(const QString &host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    QString host(QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setPort(int port);
    int port(int defaultPort = -1) const;

    void setPath(const QString &path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    QString path(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;
    QString fileName(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;

    bool hasQuery() const;
    void setQuery(const QString &query, QUrl::ParsingMode mode = QUrl::TolerantMode);
    void setQuery(const QUrlQuery &query);
    QString query(QUrl::ComponentFormattingOptions = QUrl::PrettyDecoded) const;

    bool hasFragment() const;
    QString fragment(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;
    void setFragment(const QString &fragment, QUrl::ParsingMode mode = QUrl::TolerantMode);

    Q_REQUIRED_RESULT QUrl resolved(const QUrl &relative) const;

    bool isRelative() const;
    bool isParentOf(const QUrl &url) const;

    bool isLocalFile() const;
    static Url fromLocalFile(const QString &localfile);
    QString toLocalFile() const;

    void detach();
    bool isDetached() const;

    bool operator <(const Url &url) const;
    bool operator ==(const Url &url) const;
    bool operator !=(const Url &url) const;

    bool matches(const Url &url, QUrl::FormattingOptions options) const;

    static QString fromPercentEncoding(const QByteArray& url);
    static QByteArray toPercentEncoding(
        const QString &,
        const QByteArray &exclude = QByteArray(),
        const QByteArray &include = QByteArray());

private:
    QUrl m_url;
    boost::optional<int> m_ipV6ScopeId;

    Url(const QUrl& other);

    template<typename SetUrlFunc>
    void parse(const QString& url, SetUrlFunc setUrlFunc);
};

inline quint32 qHash(const Url& url)
{
    return qHash(url.toString());
}

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

NX_UTILS_API bool equal(const nx::utils::Url& lhs, const nx::utils::Url& rhs, ComparisonFlags flags = ComparisonFlag::All);

inline bool addressesEqual(const nx::utils::Url& lhs, const nx::utils::Url& rhs)
{
    return equal(lhs, rhs, ComparisonFlag::Address);
}

} // namespace url
} // namespace utils
} // namespace nx

Q_DECLARE_METATYPE(nx::utils::Url)

inline void serialize(QnJsonContext* /*ctx*/, const nx::utils::Url& url, QJsonValue* target)
{
    *target = QJsonValue(url.toString());
}

inline bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, nx::utils::Url* target)
{
    *target = nx::utils::Url(value.toString());
    return true;
}

inline bool deserialize(const QString& s, nx::utils::Url* url)
{
    *url = nx::utils::Url(s);
    return true;
}

inline void serialize(const nx::utils::Url& url, QString* target)
{
    *target = url.toString();
}

inline QDataStream& operator<<(QDataStream& stream, const nx::utils::Url& url)
{
    stream << url.toString();
    return stream;
}

inline QDataStream& operator>>(QDataStream& stream, nx::utils::Url& url)
{
    QString urlString;
    stream >> urlString;
    url = nx::utils::Url(urlString);
    return stream;
}
