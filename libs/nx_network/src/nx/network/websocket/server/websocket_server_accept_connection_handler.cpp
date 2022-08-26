// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_server_accept_connection_handler.h"

#include "../websocket_handshake.h"

namespace nx::network::websocket::server {

AcceptConnectionHandler::AcceptConnectionHandler(
    ConnectionCreatedHandler onConnectionCreated)
    :
    base_type(
        nx::network::websocket::kWebsocketProtocolName,
        [onConnectionCreated = std::move(onConnectionCreated)](
            std::unique_ptr<AbstractStreamSocket> connection,
            http::RequestContext ctx)
        {
            auto webSocket = std::make_unique<WebSocket>(
                std::move(connection), SendMode::singleMessage, ReceiveMode::message,
                Role::server, FrameType::binary, CompressionType::perMessageDeflate);
            webSocket->start();
            onConnectionCreated(std::move(webSocket), std::move(ctx.requestPathParams));
        })
{
}

void AcceptConnectionHandler::processRequest(
    http::RequestContext requestContext,
    http::RequestProcessedHandler completionHandler)
{
    http::HttpHeaders responseHeaders;
    if (validateRequest(requestContext.request, &responseHeaders) != Error::noError)
        return completionHandler(http::StatusCode::badRequest);

    base_type::processRequest(
        std::move(requestContext),
        [completionHandler = std::move(completionHandler),
            responseHeaders = std::move(responseHeaders)](
                http::RequestResult result) mutable
        {
            for (auto& header: responseHeaders)
            {
                if (!result.headers.contains(header.first))
                    result.headers.insert(std::move(header));
            }

            completionHandler(std::move(result));
        });
}

} // namespace nx::network::websocket::server
