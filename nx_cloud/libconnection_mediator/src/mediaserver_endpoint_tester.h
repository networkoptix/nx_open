#pragma once

#include <nx/network/socket_common.h>
#include <nx/network/http/http_client.h>

#include "request_processor.h"

namespace nx {
namespace hpm {

/**
 * Mediaserver API communicating interface.
 */
class MediaserverEndpointTesterBase:
    protected RequestProcessor
{
public:
    MediaserverEndpointTesterBase(AbstractCloudDataProvider* cloudData);

    void ping(const ConnectionStrongRef& connection, network::stun::Message message);

    /**
     * Ping address to verify there is a mediaserver with expected id listening.
     */
    virtual void pingServer(
        const network::SocketAddress& address,
        const String& expectedId,
        std::function<void(network::SocketAddress, bool)> onPinged) = 0;
};

/**
 * Checks whether mediaserver with specified id is listening on the given host:port.
 * Does so by invoking /api/ping request.
 */
class MediaserverEndpointTester:
    public MediaserverEndpointTesterBase
{
public:
    MediaserverEndpointTester(AbstractCloudDataProvider* cloudData);

    virtual void pingServer(
        const network::SocketAddress& address,
        const String& expectedId,
        std::function<void(network::SocketAddress, bool)> onPinged) override;

private:
    QnMutex m_mutex;
    std::set<nx::network::http::AsyncHttpClientPtr> m_httpClients;
};

} // namespace hpm
} // namespace nx
