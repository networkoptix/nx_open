#include "user_notification_manager.h"

namespace ec2 {

QnUserNotificationManager::QnUserNotificationManager()
{
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::UserData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveUser);
    emit addedOrUpdated(tran.params, source);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::UserDataList>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveUsers);
    for (const auto& user: tran.params)
    emit addedOrUpdated(user, source);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::UserRoleData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveUserRole);
    emit userRoleAddedOrUpdated(tran.params);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeUser || tran.command == ApiCommand::removeUserRole)
    ;
    if (tran.command == ApiCommand::removeUser)
    emit removed(tran.params.id);
    else if (tran.command == ApiCommand::removeUserRole)
    emit userRoleRemoved(tran.params.id);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::AccessRightsData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::setAccessRights);
    emit accessRightsChanged(tran.params);
}

} // namespace ec2
