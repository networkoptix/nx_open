#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/network/connection_server/user_locker.h>
#include <nx/utils/std/optional.h>

#include <nx/cloud/cdb/api/result_code.h>

#include "auth_types.h"
#include "../settings.h"

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

namespace detail {

class AuthenticationHelper
{
public:
    AuthenticationHelper(
        const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
        network::server::UserLocker* userLocker,
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler handler);

    const std::string& username() const;
    const std::optional<network::http::header::DigestAuthorization>& authzHeader() const;

    bool userLocked() const;

    void reportSuccess(
        std::optional<nx::utils::stree::ResourceContainer> authProperties = std::nullopt);

    void reportFailure(
        api::ResultCode resultCode,
        std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate = std::nullopt);

    void queryStaticAuthenticationRulesTree(const StreeManager& stree);

    bool authenticatedByStaticRules() const;

    void updateUserLockoutState(network::server::UserLocker::AuthResult authResult);

    const std::function<bool(const nx::Buffer& /*ha1*/)>& validateHa1Func() const;

    bool streeQueryFoundPasswordMatchesRequestDigest();

    bool requestContainsValidDigest() const;

private:
    const nx::network::http::AuthMethodRestrictionList& m_authRestrictionList;
    network::server::UserLocker* m_userLocker;
    const nx::network::http::HttpServerConnection& m_connection;
    const nx::network::http::Request& m_request;
    nx::network::http::server::AuthenticationCompletionHandler m_handler;
    std::optional<network::http::header::DigestAuthorization> m_authzHeader;
    std::string m_username;
    std::tuple<network::HostAddress, std::string> m_userLockKey;
    std::function<bool(const nx::Buffer& /*ha1*/)> m_validateHa1Func;
    nx::utils::stree::ResourceContainer m_authTraversalResult;

    void loadAuthorizationHeader();

    nx::network::http::server::AuthenticationResult prepareSuccessResponse(
        std::optional<nx::utils::stree::ResourceContainer> authProperties = std::nullopt);

    nx::network::http::server::AuthenticationResult prepareUnauthorizedResponse(
        api::ResultCode authResult,
        std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate = std::nullopt);

    void prepareUnauthorizedResponse(
        api::ResultCode authResult,
        nx::network::http::server::AuthenticationResult* authResponse);

    bool validateNonce(const nx::network::http::StringType& nonce) const;
};

} // namespace detail

/**
 * Performs authentication based on various parameters.
 * Typically, username/digest is used to authenticate,
 *   but IP address/network interface and other data can be used also.
 * Uses account data and some predefined static data to authenticate incoming requests.
 * NOTE: Listens to user data change events.
 */
class AuthenticationManager:
    public nx::network::http::server::AbstractAuthenticationManager
{
public:
    AuthenticationManager(
        const conf::Settings& settings,
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
    std::unique_ptr<network::server::UserLocker> m_userLocker;

    api::ResultCode authenticateInDataManagers(
        const std::string& username,
        const std::function<bool(const nx::Buffer&)>& validateHa1Func,
        nx::utils::stree::ResourceContainer* const authProperties);
    nx::network::http::header::WWWAuthenticate prepareWwwAuthenticateHeader();
    nx::Buffer generateNonce();

    void reportAuthResult(
        nx::network::http::server::AuthenticationCompletionHandler handler,
        const nx::String& userId,
        nx::network::http::server::AuthenticationResult authResult);
};

} // namespace cdb
} // namespace nx
