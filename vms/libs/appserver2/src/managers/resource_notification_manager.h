// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/cleanup_db_data.h>
#include <nx/vms/api/data/license_overflow_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

class QnResourceNotificationManager: public AbstractResourceNotificationManager
{
public:
    QnResourceNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::ResourceStatusData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::GracePeriodExpirationData>& /*tran*/,
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
