#pragma once

#include <memory>
#include <mutex>
#include <tuple>
#include <map>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <QtCore/QUrl>

#include <nx/network/socket_common.h>
#include <nx/utils/url.h>
#include <nx/utils/elapsed_timer.h>

#include "auth_tools.h"
#include "http_types.h"

namespace nx {
namespace network {
namespace http {

struct AuthInfo
{
    Credentials user;
    Credentials proxyUser;
    // TODO: #ak Remove proxyEndpoint and isProxySecure from here.
    SocketAddress proxyEndpoint;
    bool isProxySecure = false;
};

/**
 * This cache is to help http clients to authenticate on server without receiving HTTP Unauthorized error first.
 */
class NX_NETWORK_API AuthInfoCache
{
public:
    class Item
    {
    public:
        nx::utils::Url url;
        StringType method;
        Credentials userCredentials;
        std::shared_ptr<header::WWWAuthenticate> wwwAuthenticateHeader;
        std::shared_ptr<header::Authorization> authorizationHeader;

        Item() = default;

        Item(
            const nx::utils::Url& url,
            const StringType& method,
            const Credentials& userCredentials,
            header::WWWAuthenticate wwwAuthenticateHeader)
            :
            url(url),
            method(method),
            userCredentials(userCredentials),
            wwwAuthenticateHeader(
                std::make_shared<header::WWWAuthenticate>(
                    std::move(wwwAuthenticateHeader)))
        {
        }
    };

    AuthInfoCache() = default;

    /**
     * Save successful authorization information in cache for later use.
     */
    void cacheAuthorization(Item item);

    /**
     * Adds Authorization header to request, if corresponding data can be found in cache.
     * @return True if necessary information has been found in cache and added to request.
     *   False otherwise.
     */
    bool addAuthorizationHeader(
        const nx::utils::Url& url,
        Request* const request,
        AuthInfoCache::Item* const item);

    /**
     * @param url Required since request->requestLine.url can contain only path.
     */
    static bool addAuthorizationHeader(
        const nx::utils::Url& url,
        Request* const request,
        AuthInfoCache::Item item);

    static AuthInfoCache* instance();

private:
    using Key = std::tuple<StringType, nx::utils::Url>;

    struct Entry
    {
        Item item;
        nx::utils::ElapsedTimer timeout;
    };

private:
    mutable std::mutex m_mutex;
    std::map<Key, Entry> m_entries;

private:
    static Key makeKey(StringType method, const nx::utils::Url& url);

    Item getCachedAuthentication(
        const nx::utils::Url& url,
        const StringType& method);

    void cleanUpStaleEntries();
};

} // namespace nx
} // namespace network
} // namespace http
