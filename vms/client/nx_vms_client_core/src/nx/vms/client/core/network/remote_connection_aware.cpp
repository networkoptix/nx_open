// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_aware.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <client_core/client_core_module.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>

namespace nx::vms::client::core {

RemoteConnectionPtr RemoteConnectionAware::connection() const
{
    if (m_mockImplementation)
        return {};

    if (auto session = qnClientCoreModule->networkModule()->session())
        return session->connection();

    return {};
}

std::shared_ptr<RemoteSession> RemoteConnectionAware::session() const
{
    if (m_mockImplementation)
        return {};

    return qnClientCoreModule->networkModule()->session();
}

ec2::AbstractECConnectionPtr RemoteConnectionAware::messageBusConnection() const
{
    if (m_mockImplementation)
        return {};

    if (auto connection = this->connection())
        return connection->messageBusConnection();

    return {};
}

nx::network::SocketAddress RemoteConnectionAware::connectionAddress() const
{
    if (m_mockImplementation)
        return m_mockImplementation->address;
        
    if (auto connection = this->connection())
        return connection->address();

    return {};
}

nx::network::http::Credentials RemoteConnectionAware::connectionCredentials() const
{
    if (m_mockImplementation)
        return m_mockImplementation->credentials;

    if (auto connection = this->connection())
        return connection->credentials();

    return {};
}

ConnectionInfo RemoteConnectionAware::connectionInfo() const
{
    if (m_mockImplementation)
        return {};

    if (auto connection = this->connection())
        return connection->connectionInfo();

    return {};
}

rest::ServerConnectionPtr RemoteConnectionAware::connectedServerApi() const
{
    if (m_mockImplementation)
        return {};

    if (auto connection = this->connection())
        return connection->serverApi();

    return {};
}

void RemoteConnectionAware::mockImplementation(RemoteConnectionAwareMock implementation)
{
    m_mockImplementation = implementation;
}

} // namespace nx::vms::client::core
