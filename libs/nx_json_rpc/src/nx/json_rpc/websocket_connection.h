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

class NX_JSON_RPC_API WebSocketConnection: public nx::network::aio::BasicPollable
{
public:
    using base_type = nx::network::aio::BasicPollable;
    using ResponseHandler = nx::MoveOnlyFunc<void(Response)>;
    using RequestHandler = nx::MoveOnlyFunc<
        void(const Request&, ResponseHandler, WebSocketConnection*)>;
    using OnDone = nx::MoveOnlyFunc<void(SystemError::ErrorCode, WebSocketConnection*)>;

    WebSocketConnection(std::unique_ptr<nx::network::websocket::WebSocket> socket, OnDone onDone);
    virtual ~WebSocketConnection() override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void start(std::weak_ptr<WebSocketConnection> self, RequestHandler requestHandler = {});
    void send(
        Request request,
        ResponseHandler handler = {},
        std::string serializedRequest = {});

    using BatchResponseHandler =
        nx::MoveOnlyFunc<void(std::vector<Response>)>;
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
    std::weak_ptr<WebSocketConnection> m_self;
    OnDone m_onDone;
    nx::network::SocketAddress m_address;
    std::unique_ptr<IncomingProcessor> m_incomingProcessor;
    std::unique_ptr<detail::OutgoingProcessor> m_outgoingProcessor;
    std::queue<rapidjson::Document> m_queuedRequests;
    std::unique_ptr<nx::network::websocket::WebSocket> m_socket;
    std::unordered_map<QString, nx::utils::Guard> m_guards;
};

} // namespace nx::json_rpc
