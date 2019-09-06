#pragma once

#include <nx/network/http/server/abstract_authentication_manager.h>

#include "auth_types.h"

namespace nx::network::http { class AuthMethodRestrictionList; }

namespace nx::cloud::db {

class AccessBlocker;
class StreeManager;
class AbstractAuthenticationDataProvider;

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
        std::vector<AbstractAuthenticationDataProvider*> authDataProviders,
        const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
        const StreeManager& stree,
        AccessBlocker* accessBlocker);

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler completionHandler) override;

    static nx::String realm();

private:
    const nx::network::http::AuthMethodRestrictionList& m_authRestrictionList;
    const StreeManager& m_stree;
    AccessBlocker* m_transportSecurityManager;
    std::vector<AbstractAuthenticationDataProvider*> m_authDataProviders;

    nx::network::http::header::WWWAuthenticate prepareWwwAuthenticateHeader();
    nx::Buffer generateNonce();
};

} // namespace nx::cloud::db
