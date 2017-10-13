#include "auth_cache.h"

#include "auth_tools.h"

namespace nx_http {

bool AuthInfoCache::addAuthorizationHeader(
    const QUrl& url,
    Request* const request,
    AuthInfoCache::AuthorizationCacheItem* const authzData )
{
    if( authzData )
    {
        *authzData = getCachedAuthentication( url, request->requestLine.method );
        return addAuthorizationHeader( url, request, *authzData );
    }
    else
    {
        return addAuthorizationHeader( url, request, getCachedAuthentication( url, request->requestLine.method ) );
    }
}

bool AuthInfoCache::addAuthorizationHeader(
    const QUrl& url,
    Request* const request,
    AuthInfoCache::AuthorizationCacheItem authzData )
{
    if( request->requestLine.method != authzData.method )
        return false;

    if( authzData.userCredentials.username != url.userName() )
        return false;

    if( authzData.authorizationHeader &&
        authzData.url == url)
    {
        //using already-known Authorization header
        nx_http::insertOrReplaceHeader(
            &request->headers,
            nx_http::HttpHeader(
                header::Authorization::NAME,
                authzData.authorizationHeader->serialized() ) );
        return true;
    }
    else if( authzData.wwwAuthenticateHeader &&
                authzData.url.host() == url.host() &&
                authzData.url.port() == url.port() )
    {
        //generating Authorization based on previously received WWW-Authenticate header
        return addAuthorization(
            request,
            authzData.userCredentials,
            *authzData.wwwAuthenticateHeader);
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

AuthInfoCache::AuthorizationCacheItem AuthInfoCache::getCachedAuthentication(
    const QUrl& url,
    const StringType& method ) const
{
    std::lock_guard<std::mutex> lk( m_mutex );

    AuthorizationCacheItem authzItem;
    if( !m_cachedAuthorization ||
        m_cachedAuthorization->method != method )
    {
        return authzItem;
    }

    if( m_cachedAuthorization->url.host() != url.host() ||
        m_cachedAuthorization->url.port() != url.port() ||
        m_cachedAuthorization->url.userName() != url.userName() )
    {
        return authzItem;
    }

    authzItem = *m_cachedAuthorization;

    if( m_cachedAuthorization->url != url )
        authzItem.authorizationHeader.reset();

    return authzItem;
}

} // namespace nx_http
