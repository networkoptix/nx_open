// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/peer_sync_time_data.h>
#include <nx_ec/ec_api_common.h>
#include <nx_ec/managers/abstract_time_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

class QnTimeNotificationManager: public AbstractTimeNotificationManager
{
public:
    void triggerNotification(
        const QnTransaction<nx::vms::api::PeerSyncTimeData> &transaction,
        NotificationSource source);
};

using QnTimeNotificationManagerPtr = std::shared_ptr<QnTimeNotificationManager> ;

} // namespace ec2
