#pragma once

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_update_data.h"
#include "transaction/transaction.h"

namespace ec2 {

    class QnUpdatesNotificationManager : public AbstractUpdatesNotificationManager
    {
    public:
        QnUpdatesNotificationManager();

        void triggerNotification(const QnTransaction<ApiUpdateUploadData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiUpdateUploadResponceData> &transaction, NotificationSource source);
        void triggerNotification(const QnTransaction<ApiUpdateInstallData> &transaction, NotificationSource source);
    };

    typedef std::shared_ptr<QnUpdatesNotificationManager> QnUpdatesNotificationManagerPtr;
    typedef QnUpdatesNotificationManager *QnUpdatesNotificationManagerRawPtr;

} // namespace ec2
