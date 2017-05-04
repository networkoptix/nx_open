/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>
#include <random>

#include <nx/network/http/server/abstract_authentication_manager.h>

#include <nx/utils/stree/stree_manager.h>

namespace nx_http {
class AuthMethodRestrictionList;
} // namespace nx_http

namespace nx {
namespace cloud {
namespace gateway {

class StreeManager;

//!Performs authentication based on various parameters
/*!
    Typically, username/digest is used to authenticate, but IP address/network interface and other data can be used also.
    Uses account data and some predefined static data to authenticate incoming requests.
    \note Listens to user data change events
 */
class AuthenticationManager:
    public nx_http::server::AbstractAuthenticationManager
{
public:
    AuthenticationManager(
        const nx_http::AuthMethodRestrictionList& authRestrictionList,
        const nx::utils::stree::StreeManager& stree);

    virtual void authenticate(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        nx_http::server::AuthenticationCompletionHandler completionHandler) override;

    static nx::String realm(); 

private:
    const nx_http::AuthMethodRestrictionList& m_authRestrictionList;
    const nx::utils::stree::StreeManager& m_stree;
    std::random_device m_rd;
    std::uniform_int_distribution<size_t> m_dist;

    bool validateNonce(const nx_http::StringType& nonce);
    void addWWWAuthenticateHeader(
        boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate );
    nx::Buffer generateNonce();
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
