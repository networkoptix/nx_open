#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

#include <nx/vms/api/data/resource_data.h>

namespace ec2 {

class QnResourceNotificationManager: public AbstractResourceNotificationManager
{
public:
    QnResourceNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::ResourceStatusData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::LicenseOverflowData>& /*tran*/,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::CleanupDatabaseData>& /*tran*/,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::ResourceParamWithRefData>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::ResourceParamWithRefDataList>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdDataList>& tran,
        NotificationSource /*source*/);
};

typedef std::shared_ptr<QnResourceNotificationManager> QnResourceNotificationManagerPtr;

} // namespace ec2
