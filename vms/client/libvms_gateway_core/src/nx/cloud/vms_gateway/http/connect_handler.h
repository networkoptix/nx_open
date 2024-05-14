// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx {

namespace network::aio {
class AsyncChannelBridge;
}

namespace cloud {
namespace gateway {

namespace conf {
class Settings;
} // namespace conf


using TunnelCreatedHandler =
    nx::utils::MoveOnlyFunc<void(std::unique_ptr<network::aio::AsyncChannelBridge>)>;

class ConnectHandler:
    public nx::network::http::RequestHandlerWithContext
{
public:
    ConnectHandler(const conf::Settings& settings, TunnelCreatedHandler tunnelCreatedHandler);

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    void connect(
        std::shared_ptr<nx::network::http::HttpServerConnection> connection,
        const network::SocketAddress& address,
        network::http::RequestProcessedHandler completionHandler);

private:
    const conf::Settings& m_settings;

    nx::network::http::Request m_request;
    TunnelCreatedHandler m_tunnelCreatedHandler;

    std::unique_ptr<network::AbstractStreamSocket> m_targetSocket;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
