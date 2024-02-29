// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_connections.h"

#include <nx/network/websocket/websocket.h>
#include <nx/reflect/json.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/vms/api/data/id_data.h>

#include "websocket_connection.h"

namespace nx::vms::json_rpc {

void WebSocketConnections::executeAsync(
    Connection* connection,
    std::unique_ptr<Executor> executor,
    nx::utils::MoveOnlyFunc<void(nx::vms::api::JsonRpcResponse)> handler,
    std::any connectionInfo)
{
    auto threadIt = connection->threads.insert(connection->threads.begin(), std::thread());
    *threadIt = std::thread(
        [this,
            executor = std::move(executor),
            handler = std::move(handler),
            weakConnection = std::weak_ptr(connection->connection),
            connectionInfo = std::move(connectionInfo),
            threadIt]() mutable
        {
            std::promise<api::JsonRpcResponse> promise;
            executor->execute(weakConnection, connectionInfo,
                [&](auto r) { promise.set_value(std::move(r)); });
            auto response = promise.get_future().get();
            executor.reset();
            if (auto connection = weakConnection.lock())
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                if (auto c = m_connections.find(connection.get()); c != m_connections.end())
                {
                    c->second.connection->post(
                        [handler = std::move(handler), response = std::move(response)]() mutable
                        {
                            handler(std::move(response));
                        });
                    threadIt->detach();
                    c->second.threads.erase(threadIt);
                }
            }
        });
}

WebSocketConnection* WebSocketConnections::addConnection(
    std::any connectionInfo, std::unique_ptr<nx::network::websocket::WebSocket> socket)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto connection = std::make_unique<WebSocketConnection>(std::move(socket),
        [this](auto connection) { removeConnection(connection); },
        [this, connectionInfo = std::move(connectionInfo)](auto request, auto handler, auto connection)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            auto it = m_connections.find(connection);
            if (it == m_connections.end())
            {
                lock.unlock();
                NX_DEBUG(this,
                    "The connection %1 is closed, request %2with method %3 is ignored",
                    connection,
                    request.id ? nx::reflect::json::serialize(*request.id) + ' ' : std::string(),
                    request.method);
                return;
            }

            for (const auto& executorCreator: m_executorCreators)
            {
                if (executorCreator->isMatched(request.method))
                {
                    executeAsync(
                        &it->second,
                        executorCreator->create(std::move(request)),
                        std::move(handler),
                        std::move(connectionInfo));
                    return;
                }
            }

            handler(api::JsonRpcResponse::makeError(request.responseId(),
                {api::JsonRpcError::methodNotFound, "Unsupported method"}));
        });
    auto connectionPtr = connection.get();
    m_connections.emplace(connectionPtr, Connection{std::move(connection)});
    lock.unlock();
    NX_VERBOSE(this, "Add connection %1", connectionPtr);
    connectionPtr->start();
    return connectionPtr;
}

void WebSocketConnections::removeConnection(WebSocketConnection* connection)
{
    NX_VERBOSE(this, "Remove connection %1", connection);
    Connection holder;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (auto it = m_connections.find(connection); it != m_connections.end())
        {
            holder = std::move(it->second);
            m_connections.erase(it);
        }
    }
    holder.stop();
}

void WebSocketConnections::addConnectionGuards(WebSocketConnection* connection, std::vector<nx::utils::Guard> guards)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (auto it = m_connections.find(connection); it != m_connections.end())
    {
        for (auto& g: guards)
            it->second.guards.insert(it->second.guards.end(), std::move(g));
    }
}

std::size_t WebSocketConnections::count() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_connections.size();
}

void WebSocketConnections::clear()
{
    std::unordered_map<WebSocketConnection*, Connection> connections;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_connections.swap(connections);
    }
    for (auto& [_, connection]: connections)
        connection.stop();
}

void WebSocketConnections::Connection::stop()
{
    for (auto& thread: threads)
        thread.detach();
    if (connection)
    {
        auto connectionPtr = connection.get();
        connectionPtr->pleaseStop([connection = std::move(connection)]() {});
    }
}

} // namespace nx::vms::json_rpc
