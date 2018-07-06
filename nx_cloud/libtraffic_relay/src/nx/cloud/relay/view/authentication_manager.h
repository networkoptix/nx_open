#pragma once

#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/server/abstract_authentication_manager.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class AuthenticationManager:
    public nx::network::http::server::AbstractAuthenticationManager
{
public:
    AuthenticationManager(const nx::network::http::AuthMethodRestrictionList& authRestrictionList);

    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler completionHandler) override;

private:
    const nx::network::http::AuthMethodRestrictionList& m_authRestrictionList;
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
