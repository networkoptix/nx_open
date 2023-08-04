// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>
#include <unordered_map>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/data/json_rpc.h>

namespace nx::network::websocket { class WebSocket; }

namespace nx::vms::json_rpc {

class IncomingProcessor;
namespace detail { class OutgoingProcessor; }

class NX_VMS_JSON_RPC_API WebSocketConnection: public nx::network::aio::BasicPollable
{
public:
    using base_type = nx::network::aio::BasicPollable;
    using ResponseHandler = nx::utils::MoveOnlyFunc<void(nx::vms::api::JsonRpcResponse)>;
    using RequestHandler = nx::utils::MoveOnlyFunc<
        void(nx::vms::api::JsonRpcRequest, ResponseHandler, WebSocketConnection*)>;
    using OnDone = nx::utils::MoveOnlyFunc<void(WebSocketConnection*)>;

    WebSocketConnection(
        std::unique_ptr<nx::network::websocket::WebSocket> socket,
        OnDone onDone,
        RequestHandler handler = nullptr);
    virtual ~WebSocketConnection() override;
    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void start();
    void send(nx::vms::api::JsonRpcRequest jsonRpcRequest, ResponseHandler handler = nullptr);

    using BatchResponseHandler =
        nx::utils::MoveOnlyFunc<void(std::vector<nx::vms::api::JsonRpcResponse>)>;
    void send(std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests,
        BatchResponseHandler handler = nullptr);

    void addGuard(const QString& id, nx::utils::Guard guard);
    void removeGuard(const QString& id);

private:
    virtual void stopWhileInAioThread() override;
    void readNextMessage();
    void readHandler(const nx::Buffer& buffer);
    void processRequest(const QJsonValue& data);
    void processQueuedRequest();
    void send(QJsonValue data);

private:
    OnDone m_onDone;
    std::unique_ptr<IncomingProcessor> m_incomingProcessor;
    std::unique_ptr<detail::OutgoingProcessor> m_outgoingProcessor;
    std::queue<QJsonValue> m_queuedRequests;
    std::unique_ptr<nx::network::websocket::WebSocket> m_socket;
    std::unordered_map<QString, nx::utils::Guard> m_guards;
};

} // namespace nx::vms::json_rpc
