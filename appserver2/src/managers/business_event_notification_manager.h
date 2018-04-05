#pragma once

#include "transaction/transaction.h"
#include "nx_ec/ec_api.h"

namespace ec2
{

class QnBusinessEventNotificationManager: public AbstractBusinessEventNotificationManager
{
public:
    QnBusinessEventNotificationManager() {}

    void triggerNotification(const QnTransaction<ApiBusinessActionData>& tran, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/);
    void triggerNotification(const QnTransaction<ApiBusinessRuleData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiResetBusinessRuleData>& tran, NotificationSource /*source*/);
};

typedef std::shared_ptr<QnBusinessEventNotificationManager> QnBusinessEventNotificationManagerPtr;

} // namespace ec2
