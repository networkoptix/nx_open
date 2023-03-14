// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

class LookupListNotificationManager: public AbstractLookupListNotificationManager
{
public:
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::LookupListData>& tran,
        NotificationSource source);
};

using LookupListNotificationManagerPtr = std::shared_ptr<LookupListNotificationManager>;

} // namespace ec2
