// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_vms_rules_manager.h>

#include "transaction/transaction.h"

namespace ec2 {

class VmsRulesNotificationManager: public AbstractVmsRulesNotificationManager
{
public:
    VmsRulesNotificationManager()
    {
    }

    void triggerNotification(
        const QnTransaction<nx::vms::api::rules::Rule>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::rules::ResetRules>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::rules::EventInfo>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::rules::ActionInfo>& tran,
        NotificationSource /*source*/);
};

typedef std::shared_ptr<VmsRulesNotificationManager> VmsRulesNotificationManagerPtr;

} // namespace ec2
