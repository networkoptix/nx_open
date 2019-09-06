#pragma once

#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_authentication_manager.h>

#include <nx/cloud/db/api/result_code.h>

#include "auth_types.h"

namespace nx::network::http {

class AuthMethodRestrictionList;
class HttpServerConnection;

} // namespace nx::network::http

namespace nx::cloud::db {

class AbstractAuthenticationDataProvider;
class AccessBlocker;
class StreeManager;

class HttpDigestAuthenticator
{
public:
    HttpDigestAuthenticator(
        const std::string& httpMethod,
        std::string authorizationHeaderValue);

    const std::string& username() const;
    const std::optional<network::http::header::DigestAuthorization>& authzHeader() const;
    const std::function<bool(const nx::Buffer& /*ha1*/)>& validateHa1Func() const;
    bool requestContainsValidDigest() const;

    api::ResultCode authenticateInDataManagers(
        const std::vector<AbstractAuthenticationDataProvider*>& authDataProviders,
        nx::utils::stree::ResourceContainer* const authProperties);

private:
    const std::string m_httpMethod;
    const std::string m_authorizationHeaderValue;
    std::optional<network::http::header::DigestAuthorization> m_authzHeader;
    std::string m_username;
    std::function<bool(const nx::Buffer& /*ha1*/)> m_validateHa1Func;

    void loadAuthorizationHeader();
    bool validateNonce(const nx::network::http::StringType& nonce) const;
};

//-------------------------------------------------------------------------------------------------

class AuthenticationHelper
{
public:
    AuthenticationHelper(
        const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
        AccessBlocker* accessBlocker,
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler handler);

    bool userLocked() const;

    bool requestContainsValidDigest() const;

    void reportSuccess(
        AuthenticationType authenticationType,
        std::optional<nx::utils::stree::ResourceContainer> authProperties = std::nullopt);

    void reportFailure(
        AuthenticationType authenticationType,
        const nx::network::http::Request request,
        api::ResultCode resultCode,
        std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate = std::nullopt);

    void queryStaticAuthenticationRulesTree(const StreeManager& stree);

    bool authenticatedByStaticRules() const;

    api::ResultCode authenticateRequestDigest(
        const std::vector<AbstractAuthenticationDataProvider*>& authDataProviders,
        nx::utils::stree::ResourceContainer* const authProperties);

private:
    const nx::network::http::AuthMethodRestrictionList& m_authRestrictionList;
    AccessBlocker* m_transportSecurityManager;
    const nx::network::http::HttpServerConnection& m_connection;
    const nx::network::http::Request& m_request;
    nx::network::http::server::AuthenticationCompletionHandler m_handler;

    HttpDigestAuthenticator m_httpDigestAuthenticator;
    std::tuple<network::HostAddress, std::string> m_userLockKey;
    nx::utils::stree::ResourceContainer m_authTraversalResult;

    nx::network::http::server::AuthenticationResult prepareSuccessResponse(
        std::optional<nx::utils::stree::ResourceContainer> authProperties = std::nullopt);

    nx::network::http::server::AuthenticationResult prepareUnauthorizedResponse(
        api::ResultCode authResult,
        std::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate = std::nullopt);

    void prepareUnauthorizedResponse(
        api::ResultCode authResult,
        nx::network::http::server::AuthenticationResult* authResponse);

    bool streeQueryFoundPasswordMatchesRequestDigest();
};

} // namespace nx::cloud::db
