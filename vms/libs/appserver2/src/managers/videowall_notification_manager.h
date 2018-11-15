#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_videowall_manager.h>

namespace ec2 {

class QnVideowallNotificationManager: public AbstractVideowallNotificationManager
{
public:
    QnVideowallNotificationManager();
    void triggerNotification(
        const QnTransaction<nx::vms::api::VideowallData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::VideowallControlMessageData>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnVideowallNotificationManager> QnVideowallNotificationManagerPtr;

} // namespace ec2
