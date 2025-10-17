// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_connections.h"

#include <nx/network/websocket/websocket.h>
#include <nx/reflect/json.h>
#include <nx/utils/json/qt_core_types.h>

#include "websocket_connection.h"

namespace nx::json_rpc {

void WebSocketConnections::executeAsync(
    Connection* connection, std::unique_ptr<Executor> executor, Handler handler)
{
    auto weakConnection = std::weak_ptr(connection->connection);
    auto h =
        [h = std::move(handler), weakConnection](auto r)
        {
            if (auto c = weakConnection.lock())
                h(std::move(r));
        };
    auto threadIt = connection->threads.insert(connection->threads.begin(), std::thread());
    *threadIt = std::thread(
        [this, executor = std::move(executor), h = std::move(h), weakConnection, threadIt]() mutable
        {
            auto handler =
                [h = std::make_shared<Handler>(std::move(h))]()
                {
                    return [h](auto r) { (*h)(std::move(r)); };
                };
            try
            {
                executor->execute(weakConnection, handler());
            }
            catch (const std::exception& e)
            {
                handler()(nx::json_rpc::Response::makeError(
                    executor->responseId(), Error::internalError,
                    NX_FMT("Unhandled exception: %1", e.what()).toStdString()));
            }
            catch (...)
            {
                handler()(nx::json_rpc::Response::makeError(
                    executor->responseId(), Error::internalError, "Unhandled exception"));
            }
            if (auto connection = weakConnection.lock())
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                if (auto c = m_connections.find(connection->id); c != m_connections.end())
                {
                    threadIt->detach();
                    c->second.threads.erase(threadIt);
                }
            }
        });
}

void WebSocketConnections::addConnection(std::shared_ptr<WebSocketConnection> connection)
{
    auto connectionPtr = connection.get();
    NX_VERBOSE(this, "Add connection %1", connectionPtr);
    connection->setRequestHandler(
        [this](auto request, auto handler, auto connection)
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
                handler(Response());
                return;
            }

            for (const auto& executorCreator: m_executorCreators)
            {
                if (executorCreator->isMatched(request.method))
                {
                    executeAsync(
                        &it->second,
                        executorCreator->create(std::move(request)),
                        std::move(handler));
                    return;
                }
            }

            lock.unlock();
            handler(Response::makeError(
                request.responseId(), Error::methodNotFound, "Unsupported method"));
        });
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_connections.emplace(connectionPtr->id, Connection{std::move(connection)});
    }
    connectionPtr->start();
}

void WebSocketConnections::removeConnection(size_t connection)
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

void WebSocketConnections::updateConnectionGuards(
    size_t connection, std::vector<nx::utils::Guard> guards)
{
    std::vector<nx::utils::Guard> oldGuards;
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (auto it = m_connections.find(connection); it != m_connections.end())
    {
        std::swap(it->second.guards, oldGuards);
        it->second.guards = std::move(guards);
    }
}

std::size_t WebSocketConnections::count() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_connections.size();
}

void WebSocketConnections::clear()
{
    std::unordered_map<size_t, Connection> connections;
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

} // namespace nx::json_rpc
