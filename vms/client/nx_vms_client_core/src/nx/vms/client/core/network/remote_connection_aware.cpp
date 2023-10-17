// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_aware.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/common/system_context.h>

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

rest::ServerConnectionPtr RemoteConnectionAware::connectedServerApi() const
{
    if (m_mockImplementation)
        return {};

    if (auto connection = this->connection())
        return connection->serverApi();

    return {};
}

void RemoteConnectionAware::setMockImplementation(
    std::unique_ptr<RemoteConnectionAwareMock> implementation)
{
    m_mockImplementation = std::move(implementation);
}

RemoteConnectionAwareMock* RemoteConnectionAware::mockImplementation() const
{
    return m_mockImplementation.get();
}

QnUuid RemoteConnectionAware::serverId() const
{
    if (auto currentConnection = connection())
        return currentConnection->moduleInformation().id;

    return QnUuid();
}

QnMediaServerResourcePtr RemoteConnectionAware::currentServer() const
{
    return qnClientCoreModule->resourcePool()->getResourceById<QnMediaServerResource>(serverId());
}

} // namespace nx::vms::client::core
