#pragma once

#include <functional>

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/utils/std/optional.h>

#include <nx/cloud/cdb/api/result_code.h>

#include "auth_types.h"

namespace nx {
namespace network {
namespace http {

class AuthMethodRestrictionList;

} // namespace nx
} // namespace network
} // namespace http

namespace nx {
namespace cdb {

class AccountManager;
class SystemManager;
class TemporaryAccountPasswordManager;
class StreeManager;
class AbstractAuthenticationDataProvider;

/**
 * Performs authentication based on various parameters.
 * Typically, username/digest is used to authenticate, but IP address/network interface and other data can be used also.
 * Uses account data and some predefined static data to authenticate incoming requests.
 * NOTE: Listens to user data change events.
 */
class AuthenticationManager:
    public nx::network::http::server::AbstractAuthenticationManager
{
public:
    AuthenticationManager(
        std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
        const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
        const StreeManager& stree);

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler completionHandler) override;

    static nx::String realm();

private:
    const nx::network::http::AuthMethodRestrictionList& m_authRestrictionList;
    const StreeManager& m_stree;
    std::vector<AbstractAuthenticationDataProvider*> m_authDataProviders;

    bool validateNonce(const nx::network::http::StringType& nonce);
    api::ResultCode authenticateInDataManagers(
        const nx::network::http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties);
    nx::network::http::header::WWWAuthenticate prepareWwwAuthenticateHeader();
    nx::Buffer generateNonce();

    nx::network::http::server::AuthenticationResult prepareSuccessResponse(
        std::optional<nx::utils::stree::ResourceContainer> authProperties = std::nullopt);

    nx::network::http::server::AuthenticationResult prepareUnauthorizedResponse(
        api::ResultCode authResult,
        std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate = std::nullopt);

    void prepareUnauthorizedResponse(
        api::ResultCode authResult,
        nx::network::http::server::AuthenticationResult* authResponse);

    bool checkUserPasswordByAuthResult(
        const std::function<bool(const nx::Buffer& /*ha1*/)>& validateHa1Func,
        const nx::String& userId,
        const nx::utils::stree::ResourceContainer& authTraversalResult);
};

} // namespace cdb
} // namespace nx
