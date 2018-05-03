#include "misc_notification_manager.h"

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(
    const QnTransaction<ApiSystemIdData> &transaction,
    NotificationSource /*source*/)
{
    emit systemIdChangeRequested(transaction.params.systemId,
        transaction.params.sysIdTime,
        transaction.params.tranLogTime);
}

void QnMiscNotificationManager::triggerNotification(const QnTransaction<ApiMiscData> &transaction)
{
    emit miscDataChanged(transaction.params.name, transaction.params.value);
}

} // namespace ec2
