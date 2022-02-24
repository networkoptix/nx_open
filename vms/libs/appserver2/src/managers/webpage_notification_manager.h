// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_webpage_manager.h>

namespace ec2 {

class QnWebPageNotificationManager: public AbstractWebPageNotificationManager
{
public:
    QnWebPageNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::WebPageData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnWebPageNotificationManager> QnWebPageNotificationManagerPtr;

} // namespace ec2
