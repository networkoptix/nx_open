// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/url.h>

#include "messages.h"

namespace nx::network::http { class AsyncClient; }
namespace nx::network::websocket { class WebSocket; }

namespace nx::json_rpc {

class WebSocketConnection;

class NX_JSON_RPC_API WebSocketClient: public nx::network::aio::BasicPollable
{
public:
    using base_type = nx::network::aio::BasicPollable;
    using ResponseHandler = nx::utils::MoveOnlyFunc<void(Response)>;
    using RequestHandler =
        nx::utils::MoveOnlyFunc<void(Request, ResponseHandler, WebSocketConnection*)>;
    using OnDone = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, WebSocketConnection*)>;

    WebSocketClient(
        nx::utils::Url url,
        nx::network::http::Credentials credentials,
        nx::network::ssl::AdapterFunc adapterFunc,
        RequestHandler handler = {});

    WebSocketClient(
        nx::utils::Url url,
        std::unique_ptr<nx::network::http::AsyncClient> client,
        RequestHandler handler = {});

    virtual ~WebSocketClient() override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    void setHandler(RequestHandler handler);
    void connectAsync(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);
    void sendAsync(Request request, ResponseHandler handler);
    void setOnDone(OnDone onDone) { m_onDone = std::move(onDone); }

private:
    virtual void stopWhileInAioThread() override;
    void onUpgrade();

private:
    std::vector<nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>> m_connectHandlers;
    nx::utils::Url m_url;
    RequestHandler m_handler;
    std::unique_ptr<nx::network::http::AsyncClient> m_handshakeClient;
    std::shared_ptr<nx::json_rpc::WebSocketConnection> m_connection;
    OnDone m_onDone;
};

} // namespace nx::json_rpc
