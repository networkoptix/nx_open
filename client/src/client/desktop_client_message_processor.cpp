#include "desktop_client_message_processor.h"

#include <core/resource_management/incompatible_server_watcher.h>
#include <core/resource/client_camera_factory.h>

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

    connect(connection->getDiscoveryNotificationManager(), &ec2::AbstractDiscoveryNotificationManager::gotInitialDiscoveredServers,
            this, &QnDesktopClientMessageProcessor::at_gotInitialDiscoveredServers);
}

void QnDesktopClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::disconnectFromConnection(connection);

    disconnect(connection->getDiscoveryNotificationManager(), nullptr, this, nullptr);
}

QnResourceFactory* QnDesktopClientMessageProcessor::getResourceFactory() const
{
    return QnClientResourceFactory::instance();
}

void QnDesktopClientMessageProcessor::at_gotInitialDiscoveredServers(
        const ec2::ApiDiscoveredServerDataList &discoveredServers)
{
    m_incompatibleServerWatcher->start();
    m_incompatibleServerWatcher->createInitialServers(discoveredServers);
}

