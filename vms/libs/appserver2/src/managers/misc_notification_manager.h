// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_misc_manager.h>
#include <transaction/transaction.h>
#include <nx/vms/api/data/system_id_data.h>

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
