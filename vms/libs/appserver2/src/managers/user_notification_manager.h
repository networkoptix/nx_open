// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2 {

class QnUserNotificationManager: public AbstractUserNotificationManager
{
public:
    QnUserNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::UserData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::UserDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::AccessRightsData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::UserRoleData>& tran,
        NotificationSource source);
};

using QnUserNotificationManagerPtr = std::shared_ptr<QnUserNotificationManager>;

} // namespace ec2
