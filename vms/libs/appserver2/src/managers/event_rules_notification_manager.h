// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_event_rules_manager.h>

#include "transaction/transaction.h"

namespace ec2 {

class QnBusinessEventNotificationManager: public AbstractBusinessEventNotificationManager
{
public:
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
