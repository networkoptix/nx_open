// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/utils/move_only_func.h>

#include "../websocket.h"

namespace nx::network::websocket::server {

class NX_NETWORK_API AcceptConnectionHandler:
    public http::server::handler::CreateTunnelHandler
{
    using base_type = http::server::handler::CreateTunnelHandler;

public:
    using ConnectionCreatedHandler =
        nx::utils::MoveOnlyFunc<void(
            std::unique_ptr<WebSocket>,
            http::RequestPathParams /*REST request parameters values*/)>;

    AcceptConnectionHandler(ConnectionCreatedHandler onConnectionCreated);

    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) override;
};

} // namespace nx::network::websocket::server
