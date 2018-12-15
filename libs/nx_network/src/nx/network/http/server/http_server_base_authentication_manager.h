#pragma once

#include <nx/utils/counter.h>

#include "abstract_authentication_manager.h"
#include "http_server_password_lookup_result.h"

namespace nx {
namespace network {
namespace http {
namespace server {

class NX_NETWORK_API AbstractAuthenticationDataProvider
{
public:
    using LookupResultHandler = nx::utils::MoveOnlyFunc<void(PasswordLookupResult)>;

    virtual ~AbstractAuthenticationDataProvider() = default;

    /**
     * Implementation is allowed to trigger completionHandler directly within this method.
     */
    virtual void getPasswordByUserName(
        const nx::String& userName,
        LookupResultHandler completionHandler) = 0;
};

class NX_NETWORK_API BaseAuthenticationManager:
    public AbstractAuthenticationManager
{
public:
    BaseAuthenticationManager(AbstractAuthenticationDataProvider* authenticationDataProvider);
    virtual ~BaseAuthenticationManager() override;

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        AuthenticationCompletionHandler completionHandler) override;

private:
    AbstractAuthenticationDataProvider* m_authenticationDataProvider;
    nx::utils::Counter m_startedAsyncCallsCounter;

    void reportAuthenticationFailure(AuthenticationCompletionHandler completionHandler);
    header::WWWAuthenticate generateWwwAuthenticateHeader();

    void passwordLookupDone(
        PasswordLookupResult passwordLookupResult,
        const nx::String& method,
        const header::DigestAuthorization& authorizationHeader,
        AuthenticationCompletionHandler completionHandler);
    void reportSuccess(AuthenticationCompletionHandler completionHandler);

    nx::String generateNonce();
    nx::String realm();
    bool validateNonce(const nx::String& nonce);
};

} // namespace server
} // namespace nx
} // namespace network
} // namespace http
