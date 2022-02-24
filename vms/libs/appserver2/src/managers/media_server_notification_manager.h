// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_media_server_manager.h>

namespace ec2 {

class QnMediaServerNotificationManager: public AbstractMediaServerNotificationManager
{
public:
    QnMediaServerNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::MediaServerData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::StorageData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::StorageDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::MediaServerUserAttributesData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::MediaServerUserAttributesDataList>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnMediaServerNotificationManager> QnMediaServerNotificationManagerPtr;

} // namespace ec2
