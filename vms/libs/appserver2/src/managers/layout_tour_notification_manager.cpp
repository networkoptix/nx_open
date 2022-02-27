// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tour_notification_manager.h"

namespace ec2 {

void QnLayoutTourNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeLayoutTour);
    emit removed(QnUuid(tran.params.id));
}

void QnLayoutTourNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::LayoutTourData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayoutTour);
    emit addedOrUpdated(tran.params, source);
}

} // namespace ec2
