#include "media_server_notification_manager.h"

namespace ec2 {

QnMediaServerNotificationManager::QnMediaServerNotificationManager()
{}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerUserAttributesDataList>& tran, NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributesList);
    for (const ec2::ApiMediaServerUserAttributesData& attrs : tran.params)
        emit userAttributesChanged(attrs);
}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerUserAttributesData>& tran, NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributes);
    emit userAttributesChanged(tran.params);
}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiIdDataList>& tran, NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeStorages);
    for (const ApiIdData& idData : tran.params)
        emit storageRemoved(idData.id);
}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::removeMediaServer)
        emit removed(tran.params.id);
    else if (tran.command == ApiCommand::removeStorage)
        emit storageRemoved(tran.params.id);
    else if (tran.command == ApiCommand::removeServerUserAttributes)
        emit userAttributesRemoved(tran.params.id);
    else
        NX_ASSERT(0, "Invalid transaction", Q_FUNC_INFO);
}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiStorageDataList>& tran, NotificationSource source)
{
    for (const auto& storage : tran.params)
        emit storageChanged(storage, source);
}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiStorageData>& tran, NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveStorage);
    emit storageChanged(tran.params, source);
}

void QnMediaServerNotificationManager::triggerNotification(const QnTransaction<ApiMediaServerData>& tran, NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveMediaServer);
    emit addedOrUpdated(tran.params, source);
}

} // namespace ec2
