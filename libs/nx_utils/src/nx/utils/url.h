#pragma once

#include <boost/optional.hpp>

#include <QtCore/QUrl>
#include <QtCore/QJsonValue>
#include <QtXml/QtXml>

#include <nx/utils/log/log.h>

namespace nx {
namespace utils {

class NX_UTILS_API Url
{
    Q_GADGET

public:
    Url();
    Url(const QString& url);
    Url& operator=(const QString& url);

    Url(const std::string& url);
    Url& operator=(const std::string& url);

    /**
     * @param url UTF-8 string.
     */
    Url(const char* url);
    Url& operator=(const char* url);

    /**
     * @param url UTF-8 string.
     */
    Url(const QByteArray& url);
    Url& operator=(const QByteArray& url);

    Url(const Url& /*other*/) = default;
    Url& operator=(const Url& /*other*/) = default;

    Url(Url&& /*other*/) = default;
    Url& operator=(Url&& /*other*/) = default;

    inline void swap(Url& other) Q_DECL_NOTHROW { m_url.swap(other.m_url); }

    void setUrl(const QString& url, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Q_INVOKABLE QString url(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    Q_INVOKABLE QString toString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    std::string toStdString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    Q_INVOKABLE QString toDisplayString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    QByteArray toEncoded(QUrl::FormattingOptions options = QUrl::FullyEncoded) const;
    Q_INVOKABLE QUrl toQUrl() const;

    static Url fromQUrl(const QUrl& url);
    static Url fromEncoded(const QByteArray &url, QUrl::ParsingMode mode = QUrl::TolerantMode);
    static Url fromUserInput(const QString &userInput);
    static Url fromUserInput(
        const QString &userInput,
        const QString &workingDirectory,
        QUrl::UserInputResolutionOptions options = QUrl::DefaultResolution);

    Q_INVOKABLE bool isValid() const;
    Q_INVOKABLE QString errorString() const;

    Q_INVOKABLE bool isEmpty() const;
    void clear();

    void setScheme(const QString &scheme);
    Q_INVOKABLE QString scheme() const;

    void setAuthority(const QString &authority, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Q_INVOKABLE QString authority(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

    void setUserInfo(const QString &userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Q_INVOKABLE QString userInfo(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

    void setUserName(const QString &userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString userName(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;

    void setPassword(const QString &password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString password(QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setHost(const QString &host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString host(QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setPort(int port);
    Q_INVOKABLE int port(int defaultPort = -1) const;

    Q_INVOKABLE QString displayAddress(
        QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setPath(const QString &path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString path(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;
    Q_INVOKABLE QString fileName(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;

    Q_INVOKABLE bool hasQuery() const;
    void setQuery(const QString &query, QUrl::ParsingMode mode = QUrl::TolerantMode);
    void setQuery(const QUrlQuery &query);
    Q_INVOKABLE QString query(QUrl::ComponentFormattingOptions = QUrl::PrettyDecoded) const;

    Q_INVOKABLE bool hasFragment() const;
    Q_INVOKABLE QString fragment(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;
    void setFragment(const QString &fragment, QUrl::ParsingMode mode = QUrl::TolerantMode);

    Q_INVOKABLE bool isRelative() const;
    Q_INVOKABLE bool isParentOf(const QUrl &url) const;

    Q_INVOKABLE bool isLocalFile() const;
    Q_INVOKABLE static Url fromLocalFile(const QString &localfile);
    Q_INVOKABLE QString toLocalFile() const;

    Q_INVOKABLE nx::utils::Url cleanUrl() const;

    bool operator <(const Url &url) const;
    bool operator ==(const Url &url) const;
    bool operator !=(const Url &url) const;

    Q_INVOKABLE bool matches(const Url &url, QUrl::FormattingOptions options) const;

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

/**
* First tries to parse strings like `hostname`, then strings like `hostname:port` and uses the QUrl
* parser for other cases.
* @return URL which may not be a valid one, so it should be checked after the call.
*/
NX_UTILS_API nx::utils::Url parseUrlFields(const QString& urlStr, QString scheme = "");

/**
 * Hides password if the string is a valid url with password. Otherwise returns original string.
 * Note: the function MUST be used for any string which may contain URL when printing to logs.
 */
NX_UTILS_API QString hidePassword(const QString& urlStr);

} // namespace url
} // namespace utils
} // namespace nx

/**
* Used only to hide passwords from logs.
*
* NOTE: Implemented as template specialization as there is some unknown circumstances which does
* not allow to just overload toString().
*/
template<>
inline QString toString<nx::utils::Url>(const nx::utils::Url& value)
{
    if (nx::utils::log::showPasswords())
        return value.toString();

    return value.toDisplayString();
}

Q_DECLARE_METATYPE(nx::utils::Url)

class QnJsonContext;

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
