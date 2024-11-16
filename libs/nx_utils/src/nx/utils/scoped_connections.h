// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

namespace nx::utils {

class ScopedConnection
{
public:
    ScopedConnection() = default;
    ScopedConnection(QMetaObject::Connection handle): m_handle(handle) {}
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&& other) noexcept: m_handle(other.release()) {}

    ~ScopedConnection() { reset(); }

    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection& operator=(ScopedConnection&&) = delete;

    void reset(QMetaObject::Connection handle = {})
    {
        QObject::disconnect(m_handle);
        m_handle = handle;
    }

    QMetaObject::Connection release()
    {
        QMetaObject::Connection result;
        std::swap(m_handle, result);
        return result;
    }

    operator bool() const { return static_cast<bool>(m_handle); }
    bool isNull() const { return !*this; }

private:
    QMetaObject::Connection m_handle;
};

class ScopedConnections
{
public:
    ScopedConnections() = default;
    ScopedConnections(const ScopedConnections&) = delete;
    ScopedConnections(ScopedConnections&& other) noexcept:
        m_connections(std::move(other.m_connections))
    {
    }

    ~ScopedConnections() = default;

    ScopedConnections& operator=(const ScopedConnections&) = delete;

    ScopedConnections& operator=(ScopedConnections&& other) noexcept
    {
        m_connections = std::move(other.m_connections);
        return *this;
    }

    ScopedConnections& add(ScopedConnection&& connection)
    {
        m_connections.push_back(std::move(connection));
        return *this;
    }

    ScopedConnections& operator<<(ScopedConnection&& connection)
    {
        return add(std::move(connection));
    }

    std::vector<ScopedConnection>::iterator begin() { return m_connections.begin(); }
    std::vector<ScopedConnection>::const_iterator begin() const { return m_connections.begin(); }

    std::vector<ScopedConnection>::iterator end() { return m_connections.begin(); }
    std::vector<ScopedConnection>::const_iterator end() const { return m_connections.begin(); }

    void reset() { m_connections.clear(); }

    void release()
    {
        for (auto& connection: m_connections)
            connection.release();

        m_connections.clear();
    }

    bool isEmpty() const
    {
        return m_connections.empty();
    }

private:
    std::vector<ScopedConnection> m_connections;
};

using ScopedConnectionsPtr = QSharedPointer<ScopedConnections>;

} // namespace nx::utils
