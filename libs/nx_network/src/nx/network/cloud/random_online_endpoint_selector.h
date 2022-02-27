// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>

#include <nx/network/socket.h>
#include <nx/utils/thread/mutex.h>

#include "endpoint_selector.h"

namespace nx::network::cloud {

/**
 * Selects any endpoint that accepts tcp connections.
 */
class NX_NETWORK_API RandomOnlineEndpointSelector:
    public AbstractEndpointSelector
    // TODO: #akolesnikov Inherit from aio::BasicPollable
{
public:
    virtual ~RandomOnlineEndpointSelector();

    virtual void selectBestEndpont(
        const std::string& moduleName,
        std::vector<SocketAddress> endpoints,
        std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> handler) override;

    /**
     * @param std::nullopt means "use default timeout".
     */
    void setTimeout(std::optional<std::chrono::milliseconds> timeout);

private:
    void done(
        AbstractStreamSocket* sock,
        SystemError::ErrorCode errorCode,
        SocketAddress endpoint);

    std::function<void(nx::network::http::StatusCode::Value, SocketAddress)> m_handler;
    bool m_endpointResolved = false;
    std::map<AbstractStreamSocket*, std::unique_ptr<AbstractStreamSocket>> m_sockets;
    size_t m_socketsStillConnecting = 0;
    mutable nx::Mutex m_mutex;
    std::optional<std::chrono::milliseconds> m_timeout;
};

} // namespace nx::network::cloud
