// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <random>

#include <nx/network/http/server/abstract_authentication_manager.h>
#include <nx/utils/random_cryptographic_device.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/stree/stree_manager.h>

namespace nx {
namespace network {
namespace http {

class AuthMethodRestrictionList;

} // namespace nx
} // namespace network
} // namespace http

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
    public nx::network::http::server::AbstractAuthenticationManager
{
    using base_type = nx::network::http::server::AbstractAuthenticationManager;

public:
    AuthenticationManager(
        nx::network::http::AbstractRequestHandler* requestHandler,
        const nx::network::http::AuthMethodRestrictionList& authRestrictionList,
        const nx::utils::stree::StreeManager& stree);

    virtual void serve(
        nx::network::http::RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(nx::network::http::RequestResult)> completionHandler) override;

    static std::string realm();

    virtual void pleaseStopSync() override {}

private:
    const nx::network::http::AuthMethodRestrictionList& m_authRestrictionList;
    const nx::utils::stree::StreeManager& m_stree;
    nx::utils::random::CryptographicDevice m_rd;
    std::uniform_int_distribution<size_t> m_dist;

    bool validateNonce(const std::string& nonce);
    nx::network::http::header::WWWAuthenticate prepareWWWAuthenticateHeader();
    nx::Buffer generateNonce();
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
