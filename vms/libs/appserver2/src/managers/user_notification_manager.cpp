// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_notification_manager.h"

namespace ec2 {

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
    const QnTransaction<nx::vms::api::UserGroupData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveUserGroup);
    emit userRoleAddedOrUpdated(tran.params);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::removeUser
        || tran.command == ApiCommand::removeUserGroup);
    if (tran.command == ApiCommand::removeUser)
        emit removed(tran.params.id, source);
    else if (tran.command == ApiCommand::removeUserGroup)
        emit userRoleRemoved(tran.params.id);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::AccessRightsDataDeprecated>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::setAccessRightsDeprecated);
    emit accessRightsChanged(tran.params);
}

void QnUserNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::UserDataDeprecated>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveUserDeprecated);
    emit addedOrUpdated(tran.params.toUserData(), source);
}

} // namespace ec2
