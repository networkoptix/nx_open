#pragma once

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "transaction/transaction.h"

namespace ec2
{

class QnStoredFileNotificationManager : public AbstractStoredFileNotificationManager
{
public:
    QnStoredFileNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::StoredFileData>& tran,
        NotificationSource /*source*/);
    void triggerNotification(
        const QnTransaction<nx::vms::api::StoredFilePath>& tran,
        NotificationSource /*source*/);
};

typedef std::shared_ptr<QnStoredFileNotificationManager> QnStoredFileNotificationManagerPtr;

} // namespace ec2
