// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <any>
#include <list>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/json_rpc.h>

namespace nx::network::websocket { class WebSocket; }

namespace nx::vms::json_rpc {

class WebSocketConnection;

class Executor
{
public:
    virtual ~Executor() = default;
    virtual void execute(
        std::weak_ptr<WebSocketConnection> connection,
        const std::any& connectionInfo,
        nx::utils::MoveOnlyFunc<void(nx::vms::api::JsonRpcResponse)> handler) = 0;
};

class ExecutorCreator
{
public:
    virtual ~ExecutorCreator() = default;
    virtual bool isMatched(const std::string& method) const = 0;
    virtual std::unique_ptr<Executor> create(nx::vms::api::JsonRpcRequest jsonRpcRequest) = 0;
};

class NX_VMS_JSON_RPC_API WebSocketConnections
{
public:
    WebSocketConnections(std::vector<std::unique_ptr<ExecutorCreator>> executorCreators):
        m_executorCreators(std::move(executorCreators))
    {
    }

    virtual ~WebSocketConnections() = default;

    WebSocketConnection* addConnection(
        std::any connectionInfo, std::unique_ptr<nx::network::websocket::WebSocket> socket);

    void removeConnection(WebSocketConnection* connection);
    void addConnectionGuards(
        WebSocketConnection* connection, std::vector<nx::utils::Guard> guards);

    std::size_t count() const;

private:
    struct Connection
    {
        std::shared_ptr<WebSocketConnection> connection;
        std::vector<nx::utils::Guard> guards;
        std::list<std::thread> threads;
    };

    void executeAsync(
        Connection* connection,
        std::unique_ptr<Executor> executor,
        nx::utils::MoveOnlyFunc<void(nx::vms::api::JsonRpcResponse)> handler,
        std::any connectionInfo);

private:
    mutable nx::Mutex m_mutex;
    std::vector<std::unique_ptr<ExecutorCreator>> m_executorCreators;
    std::unordered_map<WebSocketConnection*, Connection> m_connections;
};

} // namespace nx::vms::json_rpc
