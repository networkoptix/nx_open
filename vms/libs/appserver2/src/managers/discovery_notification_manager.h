#pragma once

#include <memory>

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

namespace ec2 {

class QnDiscoveryNotificationManager:
    public AbstractDiscoveryNotificationManager,
    public /*mixin*/ QnCommonModuleAware
{
public:
    QnDiscoveryNotificationManager(QnCommonModule* commonModule);

    void triggerNotification(
        const QnTransaction<nx::vms::api::DiscoverPeerData>& transaction, NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::DiscoveryData>& transaction, NotificationSource source);
    void triggerNotification(
        const nx::vms::api::DiscoveryData& discoveryData, bool addInformation = true);
    void triggerNotification(
        const QnTransaction<nx::vms::api::DiscoveryDataList>& tran, NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::DiscoveredServerData>& tran, NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::DiscoveredServerDataList>& tran, NotificationSource source);
};

using QnDiscoveryNotificationManagerPtr = std::shared_ptr<QnDiscoveryNotificationManager>;

} // namespace ec2
