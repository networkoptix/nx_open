#include "videowall_notification_manager.h"

namespace ec2 {

QnVideowallNotificationManager::QnVideowallNotificationManager() {}

void QnVideowallNotificationManager::triggerNotification(
    const QnTransaction<ApiVideowallData>& tran, 
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveVideowall);
    emit addedOrUpdated(tran.params, source);
}

void QnVideowallNotificationManager::triggerNotification(
    const QnTransaction<ApiIdData>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeVideowall);
    emit removed(tran.params.id);
}

void QnVideowallNotificationManager::triggerNotification(
    const QnTransaction<ApiVideowallControlMessageData>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::videowallControl);
    emit controlMessage(tran.params);
}

} // namespace ec2
