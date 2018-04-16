#pragma once

#include <transaction/transaction.h>

#include <nx_ec/managers/abstract_camera_manager.h>

namespace ec2 {

class QnCameraNotificationManager: public AbstractCameraNotificationManager
{
public:
    QnCameraNotificationManager();

    void triggerNotification(const QnTransaction<ApiCameraData>& tran, NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiCameraDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiCameraAttributesData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiCameraAttributesDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<ApiServerFootageData>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnCameraNotificationManager> QnCameraNotificationManagerPtr;

} // namespace ec2
