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

#include "messages.h"

namespace nx::json_rpc {

class WebSocketConnection;

class Executor
{
public:
    Executor(Request request): m_responseId(request.responseId()), m_request(std::move(request)) {}
    virtual ~Executor() = default;
    const ResponseId& responseId() const { return m_responseId; }

    using ResponseHandler = nx::MoveOnlyFunc<void(Response)>;
    virtual void execute(
        std::weak_ptr<WebSocketConnection> connection, ResponseHandler handler) = 0;

protected:
    ResponseId m_responseId;
    Request m_request;
};

class ExecutorCreator
{
public:
    virtual ~ExecutorCreator() = default;
    virtual bool isMatched(const std::string& method) const = 0;
    virtual std::unique_ptr<Executor> create(Request jsonRpcRequest) = 0;
};

class NX_JSON_RPC_API WebSocketConnections
{
public:
    WebSocketConnections(std::vector<std::unique_ptr<ExecutorCreator>> executorCreators):
        m_executorCreators(std::move(executorCreators))
    {
    }

    virtual ~WebSocketConnections() { clear(); }
    void clear();

    void addConnection(std::shared_ptr<WebSocketConnection> connection);
    void removeConnection(WebSocketConnection* connection);
    void updateConnectionGuards(
        WebSocketConnection* connection, std::vector<nx::utils::Guard> guards);

    std::size_t count() const;

private:
    struct Connection
    {
        std::shared_ptr<WebSocketConnection> connection;
        std::vector<nx::utils::Guard> guards;
        std::list<std::thread> threads;

        void stop();
    };

    void executeAsync(
        Connection* connection,
        std::unique_ptr<Executor> executor,
        nx::MoveOnlyFunc<void(Response)> handler);

private:
    mutable nx::Mutex m_mutex;
    std::vector<std::unique_ptr<ExecutorCreator>> m_executorCreators;
    std::unordered_map<WebSocketConnection*, Connection> m_connections;
};

} // namespace nx::json_rpc
