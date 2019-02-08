#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

#include <nx/vms/api/data/license_data.h>

namespace ec2 {

class QnLicenseNotificationManager: public AbstractLicenseNotificationManager
{
public:
    void triggerNotification(
        const QnTransaction<nx::vms::api::LicenseDataList>& tran, NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::LicenseData>& tran, NotificationSource source);
};

using QnLicenseNotificationManagerPtr = std::shared_ptr<QnLicenseNotificationManager>;

} // namespace ec2
