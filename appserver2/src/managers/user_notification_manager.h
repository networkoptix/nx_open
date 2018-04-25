#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2 {

class QnUserNotificationManager: public AbstractUserNotificationManager
{
public:
    QnUserNotificationManager();

    void triggerNotification(
        const QnTransaction<ApiUserData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiUserDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiAccessRightsData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiUserRoleData>& tran,
        NotificationSource source);
};

using QnUserNotificationManagerPtr = std::shared_ptr<QnUserNotificationManager>;

} // namespace ec2
