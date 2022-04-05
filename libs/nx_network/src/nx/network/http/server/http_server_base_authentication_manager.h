// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/counter.h>

#include "abstract_authentication_manager.h"
#include "http_server_password_lookup_result.h"

namespace nx::network::http::server {

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
 * Authenticates request using HTTP Digest authentication.
 * User credentials are taken from AbstractAuthenticationDataProvider instance.
 */
class NX_NETWORK_API BaseAuthenticationManager:
    public AbstractAuthenticationManager
{
public:
    BaseAuthenticationManager(
        AbstractAuthenticationDataProvider* authenticationDataProvider,
        Role role = Role::resourceServer);

    virtual ~BaseAuthenticationManager() override;

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        AuthenticationCompletionHandler completionHandler) override;

    std::string realm();

private:
    AbstractAuthenticationDataProvider* m_authenticationDataProvider;
    nx::utils::Counter m_startedAsyncCallsCounter;
    const Role m_role;

    void reportAuthenticationFailure(
        AuthenticationCompletionHandler completionHandler, bool isProxy);

    std::pair<std::string /*headerName*/, std::string /*headerValue*/>
        generateWwwAuthenticateHeader(bool isProxy);

    void lookupPassword(
        const nx::network::http::Request& request,
        AuthenticationCompletionHandler completionHandler,
        nx::network::http::header::DigestAuthorization authorizationHeader,
        bool isSsl);

    void validatePlainTextCredentials(
        const http::Method& method,
        const http::header::Authorization& authorizationHeader,
        const AuthToken& passwordLookupToken,
        AuthenticationCompletionHandler completionHandler);

    void reportSuccess(AuthenticationCompletionHandler completionHandler);

    std::string generateNonce();
    bool validateNonce(const std::string& nonce);
    bool isProxy(const Method& method) const;
};

} // namespace nx::network::http::server
