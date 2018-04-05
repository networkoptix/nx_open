#include "webpage_notification_manager.h"

namespace ec2 {

QnWebPageNotificationManager::QnWebPageNotificationManager()
{}

void QnWebPageNotificationManager::triggerNotification(
    const QnTransaction<ApiWebPageData> &tran, 
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveWebPage);
    emit addedOrUpdated(tran.params, source);
}

void QnWebPageNotificationManager::triggerNotification(
    const QnTransaction<ApiIdData> &tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeWebPage);
    emit removed(QnUuid(tran.params.id));
}

} // namespace ec2
