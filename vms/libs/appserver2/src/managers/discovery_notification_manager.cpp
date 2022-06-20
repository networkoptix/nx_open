// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "discovery_notification_manager.h"
#include "discovery_manager.h"

namespace ec2 {

QnDiscoveryNotificationManager::QnDiscoveryNotificationManager(
    nx::vms::discovery::Manager* discoveryManager)
    :
    m_discoveryManager(discoveryManager)
{
}

void QnDiscoveryNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::DiscoverPeerData>& transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function");

    // TODO: #rvasilenko Move it out and use signal.
    if (m_discoveryManager)
    {
        m_discoveryManager->checkEndpoint(
            nx::utils::Url(transaction.params.url), transaction.params.id);
    }

    //    emit peerDiscoveryRequested(QUrl(transaction.params.url));
}

void QnDiscoveryNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::DiscoveryData>& transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::addDiscoveryInformation ||
        transaction.command == ApiCommand::removeDiscoveryInformation,
        "Invalid command for this function");

    triggerNotification(transaction.params, transaction.command == ApiCommand::addDiscoveryInformation);
}

void QnDiscoveryNotificationManager::triggerNotification(
    const nx::vms::api::DiscoveryData& discoveryData, bool addInformation)
{
    emit discoveryInformationChanged(discoveryData, addInformation);
}

void QnDiscoveryNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::DiscoveryDataList>& tran, NotificationSource /*source*/)
{
    for (const auto& data: tran.params)
        emit discoveryInformationChanged(data, true);
}

void QnDiscoveryNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::DiscoveredServerData>& tran, NotificationSource /*source*/)
{
    emit discoveredServerChanged(tran.params);
}

void QnDiscoveryNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::DiscoveredServerDataList>& tran, NotificationSource /*source*/)
{
    emit gotInitialDiscoveredServers(tran.params);
}

} // namespace ec2
