#pragma once

#include <nx_ec/ec_api.h>
#include "transaction/transaction.h"
#include <nx_ec/data/api_license_data.h>

namespace ec2
{
    class QnLicenseNotificationManager : public AbstractLicenseNotificationManager
    {
    public:
        void triggerNotification(const QnTransaction<ApiLicenseDataList>& tran, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiLicenseData>& tran, NotificationSource source);
    };

    typedef std::shared_ptr<QnLicenseNotificationManager> QnLicenseNotificationManagerPtr;

} // namespace ec2
