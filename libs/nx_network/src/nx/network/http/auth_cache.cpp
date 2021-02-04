#include "auth_cache.h"

#include <cinttypes>

#include <nx/utils/std/algorithm.h>

#include "auth_tools.h"

namespace nx {
namespace network {
namespace http {

namespace {

const auto kEntryLifetime = std::chrono::minutes(5);

} // namespace

void AuthInfoCache::cacheAuthorization(Item item)
{
    auto key = makeKey(item.method, item.url);

    std::lock_guard lock(m_mutex);

    auto& entry = m_entries[std::move(key)];
    entry.item = std::move(item);
    entry.timeout.restart();

    cleanUpStaleEntries();
}

bool AuthInfoCache::addAuthorizationHeader(
    const nx::utils::Url& url,
    Request* const request,
    AuthInfoCache::Item* const item)
{
    if (item)
    {
        *item = getCachedAuthentication(url, request->requestLine.method);
        return addAuthorizationHeader(url, request, *item);
    }
    else
    {
        return addAuthorizationHeader(url, request, getCachedAuthentication(url, request->requestLine.method));
    }
}

bool AuthInfoCache::addAuthorizationHeader(
    const nx::utils::Url& url,
    Request* const request,
    AuthInfoCache::Item item)
{
    if (request->requestLine.method != item.method)
        return false;

    if (item.userCredentials.username != url.userName())
        return false;

    if (const auto& authenticate = item.wwwAuthenticateHeader)
    {
        if (authenticate->authScheme == header::AuthScheme::digest
            && authenticate->params.count("qop")
            && authenticate->params.count("nonce"))
        {
            auto& hex = authenticate->params["nc"];

            uint32_t nonceCount = 0;
            std::sscanf(hex.data(), "%" SCNx32, &nonceCount);

            ++nonceCount;

            hex.resize(9);
            std::snprintf(hex.data(), hex.size(), "%08" PRIx32, nonceCount);
            hex.chop(1);

            item.authorizationHeader.reset();
        }
    }

    if (item.authorizationHeader &&
        item.url == url)
    {
        // Using already-known Authorization header.
        nx::network::http::insertOrReplaceHeader(
            &request->headers,
            nx::network::http::HttpHeader(
                header::Authorization::NAME,
                item.authorizationHeader->serialized()));
        return true;
    }
    else if (item.wwwAuthenticateHeader &&
        item.url.host() == url.host() &&
        item.url.port() == url.port())
    {
        // Generating Authorization based on previously received WWW-Authenticate header.
        return addAuthorization(
            request,
            item.userCredentials,
            *item.wwwAuthenticateHeader);
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

AuthInfoCache::Key AuthInfoCache::makeKey(StringType method, const nx::utils::Url& url)
{
    nx::utils::Url keyUrl;
    keyUrl.setUserName(url.userName());
    keyUrl.setHost(url.host());
    keyUrl.setPort(url.port());
    keyUrl.setPath(url.path());

    return {std::move(method), std::move(keyUrl)};
}

AuthInfoCache::Item AuthInfoCache::getCachedAuthentication(
    const nx::utils::Url& url,
    const StringType& method)
{
    const auto key = makeKey(method, url);
    Item item;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (const auto it = m_entries.find(key); it != m_entries.end())
    {
        const auto& entry = it->second;
        item = entry.item;
    }

    cleanUpStaleEntries();

    return item;
}

void AuthInfoCache::cleanUpStaleEntries()
{
    nx::utils::remove_if(m_entries,
        [](auto&, const auto& entry)
        {
            return entry.timeout.hasExpired(kEntryLifetime);
        });
}

} // namespace nx
} // namespace network
} // namespace http
