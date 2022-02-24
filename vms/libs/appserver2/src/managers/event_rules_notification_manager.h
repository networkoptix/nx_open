// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "transaction/transaction.h"
#include <nx_ec/managers/abstract_event_rules_manager.h>

namespace ec2 {

class QnBusinessEventNotificationManager: public AbstractBusinessEventNotificationManager
{
public:
    QnBusinessEventNotificationManager()
    {
    }

    void triggerNotification(
        const QnTransaction<nx::vms::api::EventActionData>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::EventRuleData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::ResetEventRulesData>& tran,
        NotificationSource /*source*/);
};

typedef std::shared_ptr<QnBusinessEventNotificationManager> QnBusinessEventNotificationManagerPtr;

} // namespace ec2
