// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx_ec/managers/abstract_discovery_manager.h>
#include <transaction/transaction.h>
#include <common/common_module_aware.h>

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
