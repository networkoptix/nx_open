#pragma once

#include <transaction/transaction.h>

#include <nx/vms/api/data/update_data.h>

namespace ec2 {

class QnUpdatesNotificationManager : public AbstractUpdatesNotificationManager
{
public:
    QnUpdatesNotificationManager();

    void triggerNotification(const QnTransaction<nx::vms::api::UpdateUploadData>& transaction,
        NotificationSource source);
    void triggerNotification(const QnTransaction<nx::vms::api::UpdateUploadResponseData>& transaction,
        NotificationSource source);
    void triggerNotification(const QnTransaction<nx::vms::api::UpdateInstallData>& transaction,
        NotificationSource source);
};

typedef std::shared_ptr<QnUpdatesNotificationManager> QnUpdatesNotificationManagerPtr;
typedef QnUpdatesNotificationManager *QnUpdatesNotificationManagerRawPtr;

} // namespace ec2
