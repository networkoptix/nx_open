#pragma once

#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_system_name_data.h>
#include <transaction/transaction.h>

namespace ec2
{

    class QnMiscNotificationManager : public AbstractMiscNotificationManager
    {
    public:
        void triggerNotification(const QnTransaction<ApiSystemIdData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiMiscData> &transaction);
    };

    typedef std::shared_ptr<QnMiscNotificationManager> QnMiscNotificationManagerPtr;
    typedef QnMiscNotificationManager *QnMiscNotificationManagerRawPtr;

} // namespace ec2
