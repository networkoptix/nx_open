#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_videowall_manager.h>

namespace ec2
{

class QnVideowallNotificationManager : public AbstractVideowallNotificationManager
{
public:
    QnVideowallNotificationManager();
    void triggerNotification(const QnTransaction<ApiVideowallData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiVideowallControlMessageData>& tran, NotificationSource source);
};

typedef std::shared_ptr<QnVideowallNotificationManager> QnVideowallNotificationManagerPtr;

} // namespace ec2
