// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_license_manager.h>
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
