#include "updates_notification_manager.h"

using namespace nx::vms::api;

namespace ec2 {

QnUpdatesNotificationManager::QnUpdatesNotificationManager()
{
}

void QnUpdatesNotificationManager::triggerNotification(
    const QnTransaction<UpdateUploadData>& transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::uploadUpdate);
    emit updateChunkReceived(
        transaction.params.updateId, transaction.params.data, transaction.params.offset);
}

void QnUpdatesNotificationManager::triggerNotification(
    const QnTransaction<UpdateUploadResponseData>& transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::uploadUpdateResponce);
    emit updateUploadProgress(
        transaction.params.updateId, transaction.params.id, transaction.params.chunks);
}

void QnUpdatesNotificationManager::triggerNotification(
    const QnTransaction<UpdateInstallData>& transaction, NotificationSource /*source*/)
{
    NX_ASSERT(transaction.command == ApiCommand::installUpdate);
    // TODO: #2.4 #rvasilenko #dklychkov Implement a mechanism to determine the transaction
    // was successfully sent to the other peers, then emit this signal.
    emit updateInstallationRequested(transaction.params.updateId);
}

} // namespace ec2
