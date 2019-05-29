#include "authentication_manager.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

void AuthenticationManager::authenticate(
    const nx::network::http::HttpServerConnection& /*connection*/,
    const nx::network::http::Request& /*request*/,
    nx::network::http::server::AuthenticationCompletionHandler completionHandler)
{
    completionHandler(nx::network::http::server::SuccessfulAuthenticationResult());
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
