// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>

#include <nx/vms/api/data/layout_tour_data.h>

#include <nx_ec/managers/abstract_layout_tour_manager.h>

namespace ec2 {

class QnLayoutTourNotificationManager: public AbstractLayoutTourNotificationManager
{
public:
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::LayoutTourData>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnLayoutTourNotificationManager> QnLayoutTourNotificationManagerPtr;

} // namespace ec2
