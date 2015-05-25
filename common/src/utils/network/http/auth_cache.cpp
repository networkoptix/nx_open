/**********************************************************
* 25 may 2015
* a.kolesnikov
***********************************************************/

#include "auth_cache.h"

#include "auth_tools.h"


namespace nx_http
{
    AuthInfoCache::AuthInfoCache()
    {
    }

    bool AuthInfoCache::addAuthorizationHeader(
        const QUrl& url,
        Request* const request,
        AuthInfoCache::AuthorizationCacheItem* const authzData )
    {
        if( authzData )
        {
            *authzData = getCachedAuthentication( url );
            return addAuthorizationHeader( url, request, *authzData );
        }
        else
        {
            return addAuthorizationHeader( url, request, getCachedAuthentication( url ) );
        }
    }

    bool AuthInfoCache::addAuthorizationHeader(
        const QUrl& url,
        Request* const request,
        AuthInfoCache::AuthorizationCacheItem authzData )
    {
        if( authzData.authorizationHeader &&
            authzData.url == url )
        {
            //using already-known Authorization header
            nx_http::insertOrReplaceHeader(
                &request->headers,
                nx_http::HttpHeader(
                    header::Authorization::NAME,
                    authzData.authorizationHeader->toString() ) );
            return true;
        }
        else if( authzData.wwwAuthenticateHeader &&
                 authzData.url.host() == url.host() &&
                 authzData.url.port() == url.port() )
        {
            //generating Authorization based on previously received WWW-Authenticate header
            return addAuthorization(
                request,
                authzData.userName,
                authzData.password,
                authzData.ha1,
                *authzData.wwwAuthenticateHeader );
        }
        else
        {
            return false;
        }
    }

    Q_GLOBAL_STATIC(AuthInfoCache, AuthInfoCache_instance)
    AuthInfoCache* AuthInfoCache::instance()
    {
        return AuthInfoCache_instance();
    }

    AuthInfoCache::AuthorizationCacheItem AuthInfoCache::getCachedAuthentication( const QUrl& url )
    {
        std::lock_guard<std::mutex> lk( m_mutex );

        AuthorizationCacheItem authzItem;
        if( !m_cachedAuthorization )
            return authzItem;

        if( m_cachedAuthorization->url.host() != url.host() ||
            m_cachedAuthorization->url.port() != url.port() )
        {
            return authzItem;
        }

        authzItem = *m_cachedAuthorization;

        if( m_cachedAuthorization->url != url )
            authzItem.authorizationHeader.reset();

        return authzItem;
    }
}
