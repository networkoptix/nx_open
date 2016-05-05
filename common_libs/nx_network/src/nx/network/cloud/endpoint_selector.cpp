/**********************************************************
* Oct 7, 2015
* akolesnikov
***********************************************************/

#include "endpoint_selector.h"

#include <cstdlib>


namespace nx {
namespace network {
namespace cloud {

void RandomEndpointSelector::selectBestEndpont(
    const QString& /*moduleName*/,
    std::vector<SocketAddress> endpoints,
    std::function<void(nx_http::StatusCode::Value, SocketAddress)> handler)
{
    NX_ASSERT(!endpoints.empty());
    handler(
        nx_http::StatusCode::ok,
        endpoints[rand() % endpoints.size()]);
}

} // namespace cloud
} // namespace network
} // namespace nx
