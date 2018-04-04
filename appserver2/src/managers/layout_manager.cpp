#include "layout_manager.h"

namespace ec2 {

void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeLayout);
    emit removed(QnUuid(tran.params.id));
}

void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiLayoutData>& tran, NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayout);
    emit addedOrUpdated(tran.params, source);
}

void QnLayoutNotificationManager::triggerNotification(const QnTransaction<ApiLayoutDataList>& tran, NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayouts);
    for (const ApiLayoutData& layout : tran.params)
        emit addedOrUpdated(layout, source);
}

} // namespace ec2