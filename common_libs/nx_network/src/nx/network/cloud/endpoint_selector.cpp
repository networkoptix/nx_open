/**********************************************************
* Oct 7, 2015
* akolesnikov
***********************************************************/

#include "endpoint_selector.h"

#include <random>

namespace nx {
namespace network {
namespace cloud {

void RandomEndpointSelector::selectBestEndpont(
    const QString& /*moduleName*/,
    std::vector<SocketAddress> endpoints,
    std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler)
{
    std::random_device rd;
    std::default_random_engine e1(rd());
    std::uniform_int_distribution<std::vector<SocketAddress>::size_type>
        uniformDist(0, endpoints.size()-1);

    NX_ASSERT(!endpoints.empty());
    handler(
        nx_http::StatusCode::ok,
        endpoints[uniformDist(e1)]);
}

} // namespace cloud
} // namespace network
} // namespace nx
