#include "misc_notification_manager.h"

#include <nx/vms/api/data/system_id_data.h>

namespace ec2 {

void QnMiscNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::SystemIdData>& transaction, NotificationSource /*source*/)
{
    emit systemIdChangeRequested(
        transaction.params.systemId, transaction.params.sysIdTime, transaction.params.tranLogTime);
}

void QnMiscNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::MiscData>& transaction)
{
    emit miscDataChanged(transaction.params.name, transaction.params.value);
}

} // namespace ec2
