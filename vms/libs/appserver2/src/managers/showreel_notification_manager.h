// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/showreel_data.h>
#include <nx_ec/managers/abstract_showreel_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

class ShowreelNotificationManager: public AbstractShowreelNotificationManager
{
public:
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::ShowreelData>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<ShowreelNotificationManager> ShowreelNotificationManagerPtr;

} // namespace ec2
