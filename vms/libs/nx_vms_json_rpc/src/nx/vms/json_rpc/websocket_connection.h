// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <nx/network/aio/basic_pollable.h>
#include <nx/vms/api/data/json_rpc.h>

namespace nx::network::websocket { class WebSocket; }

namespace nx::vms::json_rpc {

namespace detail {

class IncomingProcessor;
class OutgoingProcessor;

} // namespace detail

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

    void send(nx::vms::api::JsonRpcRequest jsonRpcRequest, ResponseHandler handler = nullptr);

    using BatchResponseHandler =
        nx::utils::MoveOnlyFunc<void(std::vector<nx::vms::api::JsonRpcResponse>)>;
    void send(std::vector<nx::vms::api::JsonRpcRequest> jsonRpcRequests,
        BatchResponseHandler handler = nullptr);

private:
    virtual void stopWhileInAioThread() override;
    void readNextMessage();
    void readHandler(const nx::Buffer& buffer);
    void processRequest(const QJsonValue& data);
    void processQueuedRequest();
    void send(QJsonValue data);

private:
    OnDone m_onDone;
    std::unique_ptr<detail::IncomingProcessor> m_incomingProcessor;
    std::unique_ptr<detail::OutgoingProcessor> m_outgoingProcessor;
    std::queue<QJsonValue> m_queuedRequests;
    std::unique_ptr<nx::network::websocket::WebSocket> m_socket;
};

} // namespace nx::vms::json_rpc
