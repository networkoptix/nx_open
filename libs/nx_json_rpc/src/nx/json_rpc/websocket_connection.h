// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>
#include <unordered_map>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/scope_guard.h>

#include "messages.h"

namespace nx::network::websocket { class WebSocket; }

namespace nx::json_rpc {

class IncomingProcessor;
namespace detail { class OutgoingProcessor; }

class NX_JSON_RPC_API WebSocketConnection:
    public nx::network::aio::BasicPollable,
    public std::enable_shared_from_this<WebSocketConnection>
{
public:
    using base_type = nx::network::aio::BasicPollable;
    using ResponseHandler = nx::utils::MoveOnlyFunc<void(Response)>;
    using RequestHandler = nx::utils::MoveOnlyFunc<
        void(const Request&, ResponseHandler, WebSocketConnection*)>;
    using OnDone = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, WebSocketConnection*)>;

    WebSocketConnection(
        std::unique_ptr<nx::network::websocket::WebSocket> webSocket, OnDone onDone);
    virtual ~WebSocketConnection() override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void setRequestHandler(RequestHandler requestHandler);
    void start();
    void send(
        Request request,
        ResponseHandler handler = {},
        std::string serializedRequest = {});

    using BatchResponseHandler =
        nx::utils::MoveOnlyFunc<void(std::vector<Response>)>;
    void send(std::vector<Request> jsonRpcRequests,
        BatchResponseHandler handler = nullptr);

    void addGuard(const QString& id, nx::utils::Guard guard);
    void removeGuard(const QString& id);

private:
    virtual void stopWhileInAioThread() override;
    void readNextMessage();
    void readHandler(const nx::Buffer& buffer);
    void processRequest(rapidjson::Document data);
    void processQueuedRequest();
    void send(std::string data);

private:
    OnDone m_onDone;
    nx::network::SocketAddress m_address;
    std::unique_ptr<IncomingProcessor> m_incomingProcessor;
    std::unique_ptr<detail::OutgoingProcessor> m_outgoingProcessor;
    std::queue<rapidjson::Document> m_queuedRequests;
    std::unique_ptr<nx::network::websocket::WebSocket> m_socket;
    std::unordered_map<QString, nx::utils::Guard> m_guards;
};

} // namespace nx::json_rpc
