#pragma once

#include <QtCore/QUrl>

#include "url_parse_helper.h"
#include "../socket_common.h"

namespace nx::network::url {

class NX_NETWORK_API Builder
{
public:
    explicit Builder(const nx::utils::Url& url = nx::utils::Url());

    nx::utils::Url toUrl() const;
    operator nx::utils::Url() const;

    QString toString(QUrl::FormattingOptions options =
		QUrl::FormattingOptions(QUrl::PrettyDecoded)) const;

    Builder& setScheme(const QString& scheme);
    Builder& setAuthority(const QString& authority, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Builder& setUserInfo(const QString& userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Builder& setUserName(const QString& userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPassword(const QString& password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setHost(const QString& host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPort(int port);
    Builder& setEndpoint(
        const SocketAddress& endpoint,
        QUrl::ParsingMode mode = QUrl::DecodedMode);

    Builder& setPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPath(const std::string& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPath(const char* path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPath(const QByteArray& path, QUrl::ParsingMode mode = QUrl::DecodedMode);

    Builder& appendPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& appendPath(const std::string& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& appendPath(const char* path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& appendPath(const QByteArray& path, QUrl::ParsingMode mode = QUrl::DecodedMode);

    template<typename T, typename... Ts>
    Builder& appendPath(T first, Ts... tail)
    {
        appendPath(std::forward<T>(first));
        if constexpr (sizeof...(Ts) > 0) appendPath(std::forward<Ts>(tail)...);
        return *this;
    }

    template<typename T, typename... Ts>
    Builder& setPath(T first, Ts... tail)
    {
        return setPath(std::forward<T>(first)).appendPath(std::forward<Ts>(tail)...);
    }

    Builder& setQuery(const QString& query, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setQuery(const QUrlQuery& query);
    Builder& addQueryItem(const QString& name, const QString& value);

    template<typename T>
    Builder& addQueryItem(const QString& name, const T& value)
    {
        return addQueryItem(name, ::toString(value));
    }

    Builder& setFragment(const QString& fragment, QUrl::ParsingMode mode = QUrl::DecodedMode);

private:
    nx::utils::Url m_url;
};

} // namespace nx::network::url
