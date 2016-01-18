#include "desktop_client_message_processor.h"

#include <core/resource_management/incompatible_server_watcher.h>

QnDesktopClientMessageProcessor::QnDesktopClientMessageProcessor() :
    base_type(),
    m_incompatibleServerWatcher(new QnIncompatibleServerWatcher(this))
{
    connect(this, &QnClientMessageProcessor::connectionClosed,
            m_incompatibleServerWatcher, &QnIncompatibleServerWatcher::stop);
}

QnIncompatibleServerWatcher *QnDesktopClientMessageProcessor::incompatibleServerWatcher() const
{
    return m_incompatibleServerWatcher;
}

void QnDesktopClientMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::connectToConnection(connection);

    connect(connection->getDiscoveryManager(), &ec2::AbstractDiscoveryManager::gotInitialDiscoveredServers,
            this, &QnDesktopClientMessageProcessor::at_gotInitialDiscoveredServers);
}

void QnDesktopClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::disconnectFromConnection(connection);

    disconnect(connection->getDiscoveryManager(), nullptr, this, nullptr);
}

void QnDesktopClientMessageProcessor::at_gotInitialDiscoveredServers(
        const ec2::ApiDiscoveredServerDataList &discoveredServers)
{
    m_incompatibleServerWatcher->start();
    m_incompatibleServerWatcher->createInitialServers(discoveredServers);
}

