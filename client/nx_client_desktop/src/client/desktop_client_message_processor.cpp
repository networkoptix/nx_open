#include "desktop_client_message_processor.h"

#include <core/resource_management/incompatible_server_watcher.h>
#include <core/resource/client_camera_factory.h>

#include <nx_ec/data/api_full_info_data.h>

QnDesktopClientMessageProcessor::QnDesktopClientMessageProcessor(QObject* parent):
    base_type(parent),
    m_incompatibleServerWatcher(new QnIncompatibleServerWatcher())
{
    connect(this, &QnClientMessageProcessor::connectionOpened, this,
        [this]
        {
            // Watcher itself connects and disconnects from client message processor.
            m_incompatibleServerWatcher->start();
        });

    connect(this, &QnClientMessageProcessor::connectionClosed, this,
        [this]
        {
            // Watcher itself connects and disconnects from client message processor.
            m_incompatibleServerWatcher->stop();
        });
}

QnDesktopClientMessageProcessor::~QnDesktopClientMessageProcessor()
{
}

QnIncompatibleServerWatcher *QnDesktopClientMessageProcessor::incompatibleServerWatcher() const
{
    return m_incompatibleServerWatcher.data();
}

void QnDesktopClientMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::connectToConnection(connection);

    connect(connection->getDiscoveryNotificationManager(),
        &ec2::AbstractDiscoveryNotificationManager::gotInitialDiscoveredServers,
        this,
        &QnDesktopClientMessageProcessor::at_gotInitialDiscoveredServers);
}

void QnDesktopClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::disconnectFromConnection(connection);

    connection->getDiscoveryNotificationManager()->disconnect(this);
}

void QnDesktopClientMessageProcessor::onGotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    base_type::onGotInitialNotification(fullData);
}

QnResourceFactory* QnDesktopClientMessageProcessor::getResourceFactory() const
{
    return QnClientResourceFactory::instance();
}

void QnDesktopClientMessageProcessor::at_gotInitialDiscoveredServers(
        const ec2::ApiDiscoveredServerDataList &discoveredServers)
{
    m_incompatibleServerWatcher->createInitialServers(discoveredServers);
}

