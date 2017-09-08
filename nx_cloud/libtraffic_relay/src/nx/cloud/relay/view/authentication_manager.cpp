#include "authentication_manager.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

AuthenticationManager::AuthenticationManager(
    const nx_http::AuthMethodRestrictionList& authRestrictionList)
    :
    m_authRestrictionList(authRestrictionList)
{
}

void AuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& /*connection*/,
    const nx_http::Request& /*request*/,
    nx_http::server::AuthenticationCompletionHandler completionHandler)
{
    completionHandler(nx_http::server::SuccessfulAuthenticationResult());
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
