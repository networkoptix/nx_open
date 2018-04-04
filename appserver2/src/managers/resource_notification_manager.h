#pragma once

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_resource_data.h"
#include "transaction/transaction.h"

namespace ec2
{

class QnResourceNotificationManager : public AbstractResourceNotificationManager
{
public:
    QnResourceNotificationManager();

    void triggerNotification(const QnTransaction<ApiResourceStatusData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiLicenseOverflowData>& /*tran*/, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiCleanupDatabaseData>& /*tran*/, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiResourceParamWithRefData>& tran, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiResourceParamWithRefDataList>& tran, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiIdDataList>& tran, NotificationSource /*source*/);
};

typedef std::shared_ptr<QnResourceNotificationManager> QnResourceNotificationManagerPtr;

} // namespace ec2
