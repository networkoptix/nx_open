// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_server_notification_manager.h"
#include <rest/request_type_wrappers.h>
#include <nx/utils/url.h>

using namespace nx;

namespace ec2 {

QnMediaServerNotificationManager::QnMediaServerNotificationManager()
{
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<vms::api::MediaServerUserAttributesDataList>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributesList);
    for (const auto& attrs: tran.params)
        emit userAttributesChanged(attrs);
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<vms::api::MediaServerUserAttributesData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributes);
    emit userAttributesChanged(tran.params);
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdDataList>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::removeStorages);
    for (const nx::vms::api::IdData& idData: tran.params)
        emit storageRemoved(idData.id, source);
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource source)
{
    if (tran.command == ApiCommand::removeMediaServer)
        emit removed(tran.params.id, source);
    else if (tran.command == ApiCommand::removeStorage)
        emit storageRemoved(tran.params.id, source);
    else if (tran.command == ApiCommand::removeServerUserAttributes)
        emit userAttributesRemoved(tran.params.id);
    else
        NX_ASSERT(0, "Invalid transaction");
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<vms::api::StorageDataList>& tran,
    NotificationSource source)
{
    for (const auto& storage: tran.params)
    {
        NX_VERBOSE(
            this, "Emitting storageChanged signal for %1",
            nx::utils::url::hidePassword(storage.url));
        emit storageChanged(storage, source);
    }
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<vms::api::StorageData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveStorage);
    emit storageChanged(tran.params, source);
}

void QnMediaServerNotificationManager::triggerNotification(
    const QnTransaction<vms::api::MediaServerData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveMediaServer);
    emit addedOrUpdated(tran.params, source);
}

} // namespace ec2
