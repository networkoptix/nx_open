// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx_ec/managers/abstract_discovery_manager.h>
#include <transaction/transaction.h>

namespace nx::vms::discovery { class Manager; }

namespace ec2 {

class QnDiscoveryNotificationManager:
    public AbstractDiscoveryNotificationManager
{
public:
    QnDiscoveryNotificationManager(nx::vms::discovery::Manager* discoveryManager);

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

private:
    nx::vms::discovery::Manager* const m_discoveryManager;
};

using QnDiscoveryNotificationManagerPtr = std::shared_ptr<QnDiscoveryNotificationManager>;

} // namespace ec2
