#include "discovery_notification_manager.h"
#include "discovery_manager.h"

#include <common/common_module.h>

namespace ec2 {

QnDiscoveryNotificationManager::QnDiscoveryNotificationManager(QnCommonModule* commonModule) :
    QnCommonModuleAware(commonModule)
{
}

void QnDiscoveryNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::DiscoverPeerData>& transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::discoverPeer, "Invalid command for this function");

    // TODO: maybe it's better to move it out and use signal?..
    if (const auto manager = commonModule()->moduleDiscoveryManager())
        manager->checkEndpoint(nx::utils::Url(transaction.params.url), transaction.params.id);

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
