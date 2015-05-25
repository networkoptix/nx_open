/**********************************************************
* 25 may 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_AUTH_CACHE_H
#define NX_AUTH_CACHE_H

#include <memory>
#include <mutex>

#include <boost/optional.hpp>

#include <QtCore/QUrl>

#include <utils/common/singleton.h>

#include "httptypes.h"


namespace nx_http
{
    //!This cache is to help http clients to authenticate on server without receiving HTTP Unauthorized error first
    class AuthInfoCache
    {
    public:
        class AuthorizationCacheItem
        {
        public:
            QUrl url;
            StringType userName;
            boost::optional<StringType> password;
            boost::optional<BufferType> ha1;
            std::shared_ptr<header::WWWAuthenticate> wwwAuthenticateHeader;
            std::shared_ptr<header::Authorization> authorizationHeader;

            AuthorizationCacheItem() {}
            template<
                typename WWWAuthenticateRef,
                typename AuthorizationRef
            >
            AuthorizationCacheItem(
                const QUrl& url,
                const StringType& userName,
                boost::optional<StringType> password,
                boost::optional<BufferType> ha1,
                WWWAuthenticateRef&& wwwAuthenticateHeader,
                AuthorizationRef&& authorization )
            :
                url( url ),
                userName( userName ),
                password( std::move(password) ),
                ha1( std::move(ha1) ),
                wwwAuthenticateHeader(
                    std::make_shared<header::WWWAuthenticate>(
                        std::forward<WWWAuthenticateRef>(wwwAuthenticateHeader) ) ),
                authorizationHeader(
                    std::make_shared<header::Authorization>(
                        std::forward<AuthorizationRef>(authorization) ) )
            {
            }
        };


        AuthInfoCache();

        //!Save successful authorization information in cache for later use
        template<typename AuthorizationCacheItemRef>
        void cacheAuthorization( AuthorizationCacheItemRef&& item )
        {
            std::lock_guard<std::mutex> lk( m_mutex );

            m_cachedAuthorization.reset( new AuthorizationCacheItem(
                std::forward<AuthorizationCacheItemRef>(item) ) );
        }

        //!Adds Authorization header to \a request, if possible
        /*!
            \return \a true if necessary information has been found in cache and added to \a request. \a false otherwise
        */
        bool addAuthorizationHeader(
            const QUrl& url,
            Request* const request,
            AuthInfoCache::AuthorizationCacheItem* const authzData );

        static bool addAuthorizationHeader(
            Request* const request,
            AuthInfoCache::AuthorizationCacheItem authzData );
        static AuthInfoCache* instance();

    private:
        //!Authorization header, successfully used with \a m_url
        /*!
            //TODO #ak (2.4) this information should stored globally depending on server endpoint, server path, user credentials 
        */
        std::unique_ptr<AuthorizationCacheItem> m_cachedAuthorization;
        std::mutex m_mutex;

        AuthorizationCacheItem getCachedAuthentication( const QUrl& url );

        AuthInfoCache( const AuthInfoCache& );
        AuthInfoCache& operator=( const AuthInfoCache& );
    };
}

#endif  //NX_AUTH_CACHE_H
