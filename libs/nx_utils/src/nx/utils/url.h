// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <QtCore/QUrl>

#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_core_types.h>

namespace nx::utils {

class NX_UTILS_API Url
{
    Q_GADGET

public:
    Url();
    Url(const QString& url);
    Url& operator=(const QString& url);

    Url(const std::string_view& url);
    Url& operator=(const std::string_view& url);

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

    void swap(Url& other) Q_DECL_NOTHROW { m_url.swap(other.m_url); }

    void setUrl(const QString& url, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Q_INVOKABLE QString url(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    Q_INVOKABLE QString toString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    /**
     * WARNING! URL string created by this method is not complient with RFC3986#section-4.1. This is
     * a dirty work-around to fix some problems with web client. Please, think twice before using it!
     *
     * Creates string with swapped fragment and query URL fields.
     * NOTE: Creating the Url from the string produced by this method will be parsed incorrectly, if
     * the fragment field is non-empty.
     */
    QString toWebClientStandardViolatingUrl(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    std::string toStdString(
        QUrl::FormattingOptions options = QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    static Url fromStdString(const std::string& string);

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
    bool empty() const { return isEmpty(); }
    void clear();

    void setScheme(const std::string_view& scheme);

    // Emulating explicit setScheme(const QString&).
    template<typename S, typename = std::enable_if_t<std::is_same_v<S, QString>>>
    void setScheme(const S& s) { setScheme(s.toStdString()); }

    Q_INVOKABLE QString scheme() const;

    void setAuthority(const QString &authority, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Q_INVOKABLE QString authority(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

    void setUserInfo(const QString &userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Q_INVOKABLE QString userInfo(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

    void setUserName(const QString& userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setUserName(const std::string_view& userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setUserName(const char* userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString userName(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;

    void setPassword(const QString& password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setPassword(const std::string_view& password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setPassword(const char* password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString password(QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setHost(const QString& host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setHost(const std::string& host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setHost(const char* host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString host(QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setPort(int port);
    Q_INVOKABLE int port(int defaultPort = -1) const;

    Q_INVOKABLE QString displayAddress(
        QUrl::ComponentFormattingOptions = QUrl::FullyDecoded) const;

    void setPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setPath(const std::string_view& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setPath(const char* path, QUrl::ParsingMode mode = QUrl::DecodedMode);

    Q_INVOKABLE QString path(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;
    Q_INVOKABLE QString fileName(QUrl::ComponentFormattingOptions options = QUrl::FullyDecoded) const;

    Q_INVOKABLE bool hasQuery() const;
    void setQuery(const QString &query, QUrl::ParsingMode mode = QUrl::TolerantMode);
    void setQuery(const QUrlQuery &query);
    Q_INVOKABLE QString query(QUrl::ComponentFormattingOptions = QUrl::PrettyDecoded) const;
    Q_INVOKABLE bool hasQueryItem(const QString& key) const;
    Q_INVOKABLE QString queryItem(const QString& key) const;

    void setPathAndQuery(const QString& pathAndQuery, QUrl::ParsingMode mode = QUrl::DecodedMode);
    void setPathAndQuery(std::string_view pathAndQuery, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Q_INVOKABLE QString pathAndQuery(QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded) const;

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

    Q_INVOKABLE bool matches(const Url &url, QUrl::FormattingOptions options) const;

    static QString fromPercentEncoding(const QByteArray& url);
    static QString fromPercentEncoding(const std::string_view& url);
    static QString fromPercentEncoding(const char* url);

    static QByteArray toPercentEncoding(
        const QString& str,
        const QByteArray &exclude = QByteArray(),
        const QByteArray &include = QByteArray());

    static std::string toPercentEncoding(
        const std::string_view& str,
        const std::string_view& exclude = std::string_view(),
        const std::string_view& include = std::string_view());

    static bool isValidHost(const std::string_view& host);

private:
    QUrl m_url;

    Url(const QUrl& other);
};

NX_REFLECTION_TAG_TYPE(Url, useStringConversionForSerialization)

inline size_t qHash(const Url& url)
{
    return qHash(url.toString());
}

inline std::ostream& operator<<(std::ostream& s, const Url& url)
{
    return s << url.toString().toStdString();
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
 * Hides password in the url and also in the query parameters, provided they are valid urls.
 * Note: the function MUST be used for any string which may contain URL when printing to logs.
 * Note: Behavior may be altered by setting the showPasswordsInLogs nx_utils.ini to true; in this
 * case url.toString() is returned.
 */
NX_UTILS_API QString hidePassword(nx::utils::Url url);

/**
 * Compares web page hosts omitting the default "www" subdomain.
 */
NX_UTILS_API bool webPageHostsEqual(const nx::utils::Url& left, const nx::utils::Url& right);

} // namespace url

/**
* Used only to hide passwords from logs.
*/
NX_UTILS_API QString toString(const nx::utils::Url& value);

} // namespace nx::utils

class QnJsonContext;
class QJsonValue;

NX_UTILS_API void serialize(QnJsonContext* /*ctx*/, const nx::utils::Url& url, QJsonValue* target);
NX_UTILS_API bool deserialize(QnJsonContext* /*ctx*/, const QJsonValue& value, nx::utils::Url* target);

namespace std {

template<>
struct hash<nx::utils::Url>
{
    std::size_t operator()(const nx::utils::Url& k) const
    {
        return std::hash<std::string>()(k.toStdString());
    }
};

} // namespace std
