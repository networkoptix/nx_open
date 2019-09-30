#include "authentication_manager.h"

#include <nx/network/app_info.h>
#include <nx/utils/random.h>
#include <nx/utils/random_cryptographic_device.h>

#include "access_blocker.h"
#include "authentication_helper.h"

namespace nx::cloud::db {

using namespace nx::network::http;

AuthenticationManager::AuthenticationManager(
    std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
    const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
    const StreeManager& stree,
    AccessBlocker* accessBlocker)
:
    m_authRestrictionList(authRestrictionList),
    m_stree(stree),
    m_transportSecurityManager(accessBlocker),
    m_authDataProviders(std::move(authDataProviders))
{
}

void AuthenticationManager::authenticate(
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    nx::network::http::server::AuthenticationCompletionHandler handler)
{
    AuthenticationHelper authenticatorHelper(
        m_authRestrictionList,
        m_transportSecurityManager,
        connection,
        request,
        std::move(handler));

    if (authenticatorHelper.userLocked())
    {
        return authenticatorHelper.reportFailure(
            AuthenticationType::other,
            request,
            api::ResultCode::accountBlocked);
    }

    authenticatorHelper.queryStaticAuthenticationRulesTree(m_stree);
    if (authenticatorHelper.authenticatedByStaticRules())
        return authenticatorHelper.reportSuccess(AuthenticationType::other);

    if (!authenticatorHelper.requestContainsValidDigest())
    {
        return authenticatorHelper.reportFailure(
            AuthenticationType::other,
            request,
            api::ResultCode::notAuthorized,
            prepareWwwAuthenticateHeader());
    }

    nx::utils::stree::ResourceContainer authProperties;
    const auto authResultCode = authenticatorHelper.authenticateRequestDigest(
        m_authDataProviders,
        &authProperties);

    if (authResultCode == api::ResultCode::ok)
    {
        return authenticatorHelper.reportSuccess(
            AuthenticationType::credentials,
            std::move(authProperties));
    }
    else
    {
        return authenticatorHelper.reportFailure(
            AuthenticationType::credentials,
            request,
            authResultCode);
    }
}

nx::String AuthenticationManager::realm()
{
    return nx::network::AppInfo::realm().toUtf8();
}

nx::network::http::header::WWWAuthenticate
    AuthenticationManager::prepareWwwAuthenticateHeader()
{
    nx::network::http::header::WWWAuthenticate wwwAuthenticate;

    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.emplace("nonce", generateNonce());
    wwwAuthenticate.params.emplace("realm", realm());
    wwwAuthenticate.params.emplace("algorithm", "MD5");

    return wwwAuthenticate;
}

nx::Buffer AuthenticationManager::generateNonce()
{
    const auto nonce =
        nx::utils::random::number<nx::utils::random::CryptographicDevice, uint64_t>(
            nx::utils::random::CryptographicDevice::instance())
        | nx::utils::timeSinceEpoch().count();
    return nx::Buffer::number((qulonglong)nonce);
}

} // namespace nx::cloud::db
