/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <functional>
#include <random>

#include <nx/network/http/server/abstract_authentication_manager.h>

#include <plugins/videodecoder/stree/stree_manager.h>


class QnAuthMethodRestrictionList;

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
class AuthenticationManager
:
    public nx_http::AbstractAuthenticationManager
{
public:
    AuthenticationManager(
        const QnAuthMethodRestrictionList& authRestrictionList,
        const stree::StreeManager& stree);

    virtual bool authenticate(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        boost::optional<nx_http::header::WWWAuthenticate>* const wwwAuthenticate,
        stree::ResourceContainer* authProperties,
        nx_http::HttpHeaders* const responseHeaders,
        std::unique_ptr<nx_http::AbstractMsgBodySource>* const msgBody) override;
        
    static nx::String realm(); 

private:
    const QnAuthMethodRestrictionList& m_authRestrictionList;
    const stree::StreeManager& m_stree;
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
