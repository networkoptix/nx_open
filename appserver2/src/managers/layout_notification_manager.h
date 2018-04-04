#pragma once

#include <transaction/transaction.h>
#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>

namespace ec2
{

class QnLayoutNotificationManager: public AbstractLayoutNotificationManager
{
public:
    void triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiLayoutData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiLayoutDataList>& tran, NotificationSource source);
};

typedef std::shared_ptr<QnLayoutNotificationManager> QnLayoutNotificationManagerPtr;

} // namespace ec2
