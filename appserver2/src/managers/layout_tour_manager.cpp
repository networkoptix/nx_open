#include "layout_tour_manager.h"

namespace ec2 {

void QnLayoutTourNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeLayoutTour);
    emit removed(QnUuid(tran.params.id));
}

void QnLayoutTourNotificationManager::triggerNotification(
    const QnTransaction<ApiLayoutTourData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayoutTour);
    emit addedOrUpdated(tran.params, source);
}

} // namespace ec2