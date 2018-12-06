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
    ScopedConnection(ScopedConnection&& other): m_handle(other.release()) {}

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
    ScopedConnections(ScopedConnections&& other): m_connections(std::move(other.m_connections)) {}

    ~ScopedConnections() = default;

    ScopedConnections& operator=(const ScopedConnections&) = delete;

    ScopedConnections& operator=(ScopedConnections&& other)
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

    void reset() { m_connections.clear(); }

private:
    std::vector<ScopedConnection> m_connections;
};

using ScopedConnectionsPtr = QSharedPointer<ScopedConnections>;

} // namespace nx::utils
