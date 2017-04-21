#pragma once

#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/server/abstract_authentication_manager.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class AuthenticationManager:
    public nx_http::server::AbstractAuthenticationManager
{
public:
    AuthenticationManager(const QnAuthMethodRestrictionList& authRestrictionList);

    virtual void authenticate(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        nx_http::server::AuthenticationCompletionHandler completionHandler) override;

private:
    const QnAuthMethodRestrictionList& m_authRestrictionList;
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
