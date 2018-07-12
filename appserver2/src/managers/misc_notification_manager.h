#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

#include <nx/vms/api/data_fwd.h>

namespace ec2 {

class QnMiscNotificationManager: public AbstractMiscNotificationManager
{
public:
    void triggerNotification(const QnTransaction<nx::vms::api::SystemIdData>& transaction,
        NotificationSource source);
    void triggerNotification(const QnTransaction<nx::vms::api::MiscData>& transaction);
};

using QnMiscNotificationManagerPtr = std::shared_ptr<QnMiscNotificationManager>;
using QnMiscNotificationManagerRawPtr = QnMiscNotificationManager*;

} // namespace ec2
