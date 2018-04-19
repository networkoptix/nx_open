#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_server_manager.h>

namespace ec2 {

class QnMediaServerNotificationManager: public AbstractMediaServerNotificationManager
{
public:
    QnMediaServerNotificationManager();

    void triggerNotification(
        const QnTransaction<ApiMediaServerData>& tran,
        NotificationSource source);
    void triggerNotification(const QnTransaction<ApiStorageData>& tran, NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiStorageDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiMediaServerUserAttributesData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiMediaServerUserAttributesDataList>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnMediaServerNotificationManager> QnMediaServerNotificationManagerPtr;

} // namespace ec2
