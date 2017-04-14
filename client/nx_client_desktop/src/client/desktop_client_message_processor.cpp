#include "desktop_client_message_processor.h"

#include <core/resource_management/incompatible_server_watcher.h>
#include <core/resource/client_camera_factory.h>

#include <core/resource_management/layout_tour_manager.h>

#include <nx_ec/data/api_full_info_data.h>

QnDesktopClientMessageProcessor::QnDesktopClientMessageProcessor(QObject* parent):
    base_type(parent),
    m_incompatibleServerWatcher(new QnIncompatibleServerWatcher())
{
    connect(this, &QnClientMessageProcessor::connectionClosed,
            m_incompatibleServerWatcher, &QnIncompatibleServerWatcher::stop);
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

    auto layoutTourManager = connection->getLayoutTourNotificationManager();

    connect(layoutTourManager,
        &ec2::AbstractLayoutTourNotificationManager::addedOrUpdated,
        qnLayoutTourManager,
        &QnLayoutTourManager::addOrUpdateTour,
        Qt::DirectConnection);

    connect(layoutTourManager,
        &ec2::AbstractLayoutTourNotificationManager::removed,
        qnLayoutTourManager,
        &QnLayoutTourManager::removeTour,
        Qt::DirectConnection);
}

void QnDesktopClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection)
{
    base_type::disconnectFromConnection(connection);

    connection->getDiscoveryNotificationManager()->disconnect(this);
    connection->getLayoutTourNotificationManager()->disconnect(this);
    qnLayoutTourManager->resetTours();
}

void QnDesktopClientMessageProcessor::onGotInitialNotification(const ec2::ApiFullInfoData& fullData)
{
    //TODO: #GDM #3.1 logic is not perfect, we've got tours after initialResourcesReceived. Also: who will clean them on disconnect?
    base_type::onGotInitialNotification(fullData);
    qnLayoutTourManager->resetTours(fullData.layoutTours);
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

