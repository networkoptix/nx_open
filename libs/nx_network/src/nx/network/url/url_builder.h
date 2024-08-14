// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/utils/log/to_string.h>
#include <nx/utils/url.h>
#include <nx/utils/url_query.h>

#include "../socket_common.h"
#include "url_parse_helper.h"

namespace nx::network::url {

class NX_NETWORK_API Builder
{
public:
    Builder(const nx::utils::Url& url = nx::utils::Url());

    nx::utils::Url toUrl() const;
    operator nx::utils::Url() const;

    Builder& setScheme(const QString& scheme);
    Builder& setAuthority(const std::string& authority, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Builder& setUserInfo(const std::string& userInfo, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Builder& setUserName(const QString& userName, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPassword(const QString& password, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setHost(const QString& host, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPort(int port);
    Builder& setEndpoint(
        const SocketAddress& endpoint,
        QUrl::ParsingMode mode = QUrl::DecodedMode);

    Builder& setPath(const std::string_view& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& setPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode);

    Builder& appendPath(const std::string_view& path, QUrl::ParsingMode mode = QUrl::DecodedMode);
    Builder& appendPath(const QString& path, QUrl::ParsingMode mode = QUrl::DecodedMode);

    template<typename T, typename... Ts>
    Builder& appendPathParts(T first, Ts... tail)
    {
        appendPath(std::forward<T>(first));
        if constexpr (sizeof...(Ts) > 0) appendPath(std::forward<Ts>(tail)...);
        return *this;
    }

    template<typename T, typename... Ts>
    Builder& setPathParts(T first, Ts... tail)
    {
        return setPath(std::forward<T>(first)).appendPathParts(std::forward<Ts>(tail)...);
    }

    Builder& setQuery(const QString& query, QUrl::ParsingMode mode = QUrl::TolerantMode);
    Builder& setQuery(const QUrlQuery& query);
    Builder& setQuery(const nx::utils::UrlQuery& query);

    Builder& addQueryItem(const QString& name, const QString& value = {});
    Builder& removeQueryItem(const QString& name);

    template<typename String, typename T>
    Builder& addQueryItem(const String& name, const T& value)
    {
        return addQueryItem(nx::toString(name), nx::toString(value));
    }

    Builder& setFragment(const QString& fragment, QUrl::ParsingMode mode = QUrl::TolerantMode);

    Builder& setPathAndQuery(const QString& pathAndQuery);

    //---------------------------------------------------------------------------------------------
    // Convenience overloads.

    template<typename String>
    Builder& setScheme(const String& str)
    {
        return setScheme(nx::toString(str));
    }

    template<typename String>
    Builder& setUserName(const String& str, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        return setUserName(nx::toString(str), mode);
    }

    template<typename String>
    Builder& setPassword(const String& str, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        return setPassword(nx::toString(str), mode);
    }

    template<typename String>
    Builder& setHost(const String& str, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        return setHost(nx::toString(str), mode);
    }

    template<typename String>
    Builder& setPath(const String& str, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        return setPath(nx::toString(str), mode);
    }

    template<typename String>
    Builder& appendPath(const String& str, QUrl::ParsingMode mode = QUrl::DecodedMode)
    {
        return appendPath(nx::toString(str), mode);
    }

    template<typename String>
    Builder& setQuery(const String& str, QUrl::ParsingMode mode = QUrl::TolerantMode)
    {
        return setQuery(nx::toString(str), mode);
    }

    template<typename String>
    Builder& setFragment(const String& str, QUrl::ParsingMode mode = QUrl::TolerantMode)
    {
        return setFragment(nx::toString(str), mode);
    }

private:
    nx::utils::Url m_url;
};

} // namespace nx::network::url
