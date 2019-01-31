#pragma once

#include <transaction/transaction.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>

namespace ec2 {

class QnLayoutNotificationManager: public AbstractLayoutNotificationManager
{
public:
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::LayoutData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::LayoutDataList>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnLayoutNotificationManager> QnLayoutNotificationManagerPtr;

} // namespace ec2
