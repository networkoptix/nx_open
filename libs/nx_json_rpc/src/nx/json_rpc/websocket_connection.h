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

struct NX_JSON_RPC_API WebSocketConnectionStats
{
    std::vector<QString> guards;
    size_t inRequests = 0;
    size_t outRequests = 0;
    size_t totalInB = 0;
    size_t totalOutB = 0;
};

class NX_JSON_RPC_API WebSocketConnection: public nx::network::aio::BasicPollable
{
public:
    using base_type = nx::network::aio::BasicPollable;
    using ResponseHandler = nx::MoveOnlyFunc<void(Response)>;
    using RequestHandler = nx::MoveOnlyFunc<void(Request, ResponseHandler, size_t)>;
    using OnDone = nx::MoveOnlyFunc<void(SystemError::ErrorCode, size_t)>;

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
        nx::MoveOnlyFunc<void(std::vector<Response>)>;
    void send(std::vector<Request> jsonRpcRequests,
        BatchResponseHandler handler = nullptr);

    void addGuard(const QString& id, nx::utils::Guard guard);
    void removeGuard(const QString& id);

    WebSocketConnectionStats stats() const;

    const size_t id;

private:
    virtual void stopWhileInAioThread() override;
    void readNextMessage();
    void readHandler(const nx::Buffer& buffer);
    void processRequest(rapidjson::Document data);
    void processQueuedRequest();
    void send(std::string data);

private:
    std::atomic_bool m_stopped = false;
    std::unordered_map<QString, nx::utils::Guard> m_guards;
    OnDone m_onDone;
    nx::network::SocketAddress m_address;
    std::unique_ptr<IncomingProcessor> m_incomingProcessor;
    std::unique_ptr<detail::OutgoingProcessor> m_outgoingProcessor;
    std::queue<rapidjson::Document> m_queuedRequests;
    std::unique_ptr<nx::network::websocket::WebSocket> m_socket;
    size_t m_inRequests = 0;
    size_t m_outRequests = 0;
};

} // namespace nx::json_rpc
