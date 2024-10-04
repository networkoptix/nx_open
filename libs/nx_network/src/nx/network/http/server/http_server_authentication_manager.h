// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/counter.h>

#include "abstract_authentication_manager.h"
#include "http_server_password_lookup_result.h"

namespace nx::network::http::server {

/**
 * Interface of a class providing credentials to AuthenticationManager.
 */
class NX_NETWORK_API AbstractAuthenticationDataProvider
{
public:
    using LookupResultHandler = nx::utils::MoveOnlyFunc<void(PasswordLookupResult)>;

    virtual ~AbstractAuthenticationDataProvider() = default;

    /**
     * Implementation is allowed to trigger completionHandler directly within this method.
     */
    virtual void getPasswordByUserName(
        const std::string& userName,
        LookupResultHandler completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Implements standard HTTP authentication.
 * Supports:
 * - HTTP Digest over SSL and non-SSL connections
 * - HTTP Basic over SSL only
 * - Authorization and Proxy-Authorization headers
 *
 * User credentials are taken from AbstractAuthenticationDataProvider instance.
 * For SSL connection detection nx::network::http::server::rest::HttpsRequestDetector class is used.
 * On authentication success the request is passed down the processing pipeline, to the next handler.
 * On authentication failure, 401 HTTP status is reported.
 */
class NX_NETWORK_API AuthenticationManager:
    public AbstractAuthenticationManager
{
    using base_type = AbstractAuthenticationManager;

public:
    /**
     * @param nextHandler Receives the request after successful authentication.
     * @param authenticationDataProvider Provides password based on the username found in the request.
     * @param role Server role (resource server/proxy)
     */
    AuthenticationManager(
        AbstractRequestHandler* nextHandler,
        AbstractAuthenticationDataProvider* authenticationDataProvider,
        Role role = Role::resourceServer);

    virtual ~AuthenticationManager() override;

    virtual void serve(
        RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler) override;

    std::string realm();

    virtual void pleaseStopSync() override;

private:
    AbstractAuthenticationDataProvider* m_authenticationDataProvider;
    nx::utils::Counter m_startedAsyncCallsCounter;
    const Role m_role;

    void reportAuthenticationFailure(
        AuthenticationCompletionHandler completionHandler, bool isProxy);

    std::pair<std::string /*headerName*/, std::string /*headerValue*/>
        generateWwwAuthenticateHeader(bool isProxy);

    void lookupPassword(
        RequestContext requestContext,
        AuthenticationCompletionHandler completionHandler,
        nx::network::http::header::DigestAuthorization authorizationHeader,
        bool isSsl);

    void validatePlainTextCredentials(
        RequestContext requestContext,
        const http::header::Authorization& authorizationHeader,
        const AuthToken& passwordLookupToken,
        AuthenticationCompletionHandler completionHandler);

    void reportSuccess(
        RequestContext requestContext,
        AuthenticationCompletionHandler completionHandler);

    std::string generateNonce();
    bool validateNonce(const std::string& nonce);
    bool isProxy(const Method& method) const;
};

} // namespace nx::network::http::server
