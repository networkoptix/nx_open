#pragma once

#include <nx/network/socket_common.h>
#include <nx/network/http/http_client.h>

#include "request_processor.h"

namespace nx {

namespace stun { class MessageDispatcher; }

namespace hpm {

/**
 * Mediaserver API communicating interface.
 */
class MediaserverApiBase:
    protected RequestProcessor
{
public:
    MediaserverApiBase(
        AbstractCloudDataProvider* cloudData,
        nx::stun::MessageDispatcher* dispatche);

    void ping(const ConnectionStrongRef& connection, stun::Message message);

    /**
     * Pings address and verifies if the is the mediaservers with expectedId.
     */
    virtual void pingServer(
        const SocketAddress& address,
        const String& expectedId,
        std::function<void(SocketAddress, bool)> onPinged) = 0;
};

/**
 * Mediaserver API communicating interface over nx_http::AsyncHttpClient.
 */
class MediaserverApi:
    public MediaserverApiBase
{
public:
    MediaserverApi(
        AbstractCloudDataProvider* cloudData,
        nx::stun::MessageDispatcher* dispatcher);

    virtual void pingServer(
        const SocketAddress& address,
        const String& expectedId,
        std::function<void(SocketAddress, bool)> onPinged) override;

private:
    QnMutex m_mutex;
    std::set<nx_http::AsyncHttpClientPtr> m_httpClients;
};

} // namespace hpm
} // namespace nx
