#include "stored_file_notification_manager.h"

namespace ec2
{

QnStoredFileNotificationManager::QnStoredFileNotificationManager() 
{
}

void QnStoredFileNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::StoredFileData>& tran, 
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::addStoredFile)
    {
        emit added(tran.params.path);
    }
    else if (tran.command == ApiCommand::updateStoredFile)
    {
        emit updated(tran.params.path);
    }
    else
    {
        NX_ASSERT(false);
    }
}

void QnStoredFileNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::StoredFilePath>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeStoredFile);
    emit removed(tran.params.path);
}

} // namespace ec2
