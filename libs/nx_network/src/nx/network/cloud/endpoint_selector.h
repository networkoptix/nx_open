#pragma once

#include <functional>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/network/http/http_types.h>

#include <QtCore/QString>

namespace nx {
namespace network {
namespace cloud {

/**
 * Selects endpoint that is "best" due to some logic.
 */
class NX_NETWORK_API AbstractEndpointSelector
{
public:
    virtual ~AbstractEndpointSelector() {}

    /**
     * NOTE: Multiple calls are allowed.
     */
    virtual void selectBestEndpont(
        const QString& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler) = 0;
};

class NX_NETWORK_API RandomEndpointSelector:
    public AbstractEndpointSelector
{
public:
    /**
     * @param handler Called directly in this method.
     */
    virtual void selectBestEndpont(
        const QString& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler) override;
};

} // namespace cloud
} // namespace network
} // namespace nx
