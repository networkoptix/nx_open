#include "endpoint_selector.h"

#include <cstdlib>

#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace cloud {

void RandomEndpointSelector::selectBestEndpont(
    const QString& /*moduleName*/,
    std::vector<SocketAddress> endpoints,
    std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler)
{
    NX_ASSERT(!endpoints.empty());
    handler(
        nx::network::http::StatusCode::ok,
        nx::utils::random::choice(endpoints));
}

} // namespace cloud
} // namespace network
} // namespace nx
