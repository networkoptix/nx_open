#pragma once

#include <memory>
#include <mutex>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <QtCore/QUrl>

#include <nx/network/socket_common.h>
#include <nx/utils/url.h>

#include "auth_tools.h"
#include "http_types.h"

namespace nx_http {

struct AuthInfo
{
    Credentials user;
    Credentials proxyUser;
    // TODO: #ak Remove proxyEndpoint from here.
    SocketAddress proxyEndpoint;
};

//!This cache is to help http clients to authenticate on server without receiving HTTP Unauthorized error first
class NX_NETWORK_API AuthInfoCache
{
public:
    class AuthorizationCacheItem
    {
    public:
        nx::utils::Url url;
        StringType method;
        Credentials userCredentials;
        std::shared_ptr<header::WWWAuthenticate> wwwAuthenticateHeader;
        std::shared_ptr<header::Authorization> authorizationHeader;

        AuthorizationCacheItem() = default;

        AuthorizationCacheItem(
            const nx::utils::Url& url,
            const StringType& method,
            const Credentials& userCredentials,
            header::WWWAuthenticate wwwAuthenticateHeader,
            header::Authorization authorization)
        :
            url(url),
            method(method),
            userCredentials(userCredentials),
            wwwAuthenticateHeader(
                std::make_shared<header::WWWAuthenticate>(
                    std::move(wwwAuthenticateHeader))),
            authorizationHeader(
                std::make_shared<header::Authorization>(
                    std::move(authorization)))
        {
        }

        AuthorizationCacheItem(AuthorizationCacheItem&& right) = default;
        AuthorizationCacheItem(const AuthorizationCacheItem& right) = default;
        AuthorizationCacheItem& operator=(const AuthorizationCacheItem&) = default;
        AuthorizationCacheItem& operator=(AuthorizationCacheItem&&) = default;
    };

    AuthInfoCache() = default;

    //!Save successful authorization information in cache for later use
    template<typename AuthorizationCacheItemRef>
    void cacheAuthorization( AuthorizationCacheItemRef&& item )
    {
        std::lock_guard<std::mutex> lk( m_mutex );

        m_cachedAuthorization.reset( new AuthorizationCacheItem(
            std::forward<AuthorizationCacheItemRef>(item) ) );
    }

    //!Adds Authorization header to \a request, if corresponding data can be found in cache
    /*!
        \return \a true if necessary information has been found in cache and added to \a request. \a false otherwise
    */
    bool addAuthorizationHeader(
        const nx::utils::Url& url,
        Request* const request,
        AuthInfoCache::AuthorizationCacheItem* const authzData );

    /*!
        \param url This argument is required since \a request->requestLine.url can contain only path
    */
    static bool addAuthorizationHeader(
        const nx::utils::Url& url,
        Request* const request,
        AuthInfoCache::AuthorizationCacheItem authzData );
    static AuthInfoCache* instance();

private:
    //!Authorization header, successfully used with \a m_url
    /*!
        //TODO #ak (2.4) this information should stored globally depending on server endpoint, server path, user credentials
    */
    std::unique_ptr<AuthorizationCacheItem> m_cachedAuthorization;
    mutable std::mutex m_mutex;

    AuthorizationCacheItem getCachedAuthentication(
        const nx::utils::Url& url,
        const StringType& method ) const;

    AuthInfoCache( const AuthInfoCache& );
    AuthInfoCache& operator=( const AuthInfoCache& );
};

} // namespace nx_http
